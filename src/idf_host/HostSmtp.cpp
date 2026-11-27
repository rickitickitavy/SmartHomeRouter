#include "HostSmtp.h"

#if defined(BOARD_ROLE_HOST)

#include "Defines.h"
#include "Logger.h"
#include "SettingsManager.h"

#include "esp_heap_caps.h"
#include "mbedtls/base64.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

struct SmtpSession {
    mbedtls_net_context net;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctrDrbg;
    bool useSsl = false;
    bool sslReady = false;
};

void smtpInit(SmtpSession *session) {
    mbedtls_net_init(&session->net);
    mbedtls_ssl_init(&session->ssl);
    mbedtls_ssl_config_init(&session->conf);
    mbedtls_entropy_init(&session->entropy);
    mbedtls_ctr_drbg_init(&session->ctrDrbg);
}

void smtpFree(SmtpSession *session) {
    if (session->sslReady) {
        mbedtls_ssl_close_notify(&session->ssl);
    }
    mbedtls_net_free(&session->net);
    mbedtls_ssl_free(&session->ssl);
    mbedtls_ssl_config_free(&session->conf);
    mbedtls_ctr_drbg_free(&session->ctrDrbg);
    mbedtls_entropy_free(&session->entropy);
}

int smtpWrite(SmtpSession *session, const unsigned char *buf, size_t len) {
    if (session->sslReady) {
        return mbedtls_ssl_write(&session->ssl, buf, len);
    }
    return mbedtls_net_send(&session->net, buf, len);
}

int smtpRead(SmtpSession *session, unsigned char *buf, size_t len) {
    if (session->sslReady) {
        return mbedtls_ssl_read(&session->ssl, buf, len);
    }
    return mbedtls_net_recv(&session->net, buf, len);
}

bool smtpSendRaw(SmtpSession *session, const char *text) {
    if (text == nullptr) {
        return false;
    }
    const size_t length = strlen(text);
    size_t sent = 0;
    while (sent < length) {
        const int n = smtpWrite(session, reinterpret_cast<const unsigned char *>(text + sent), length - sent);
        if (n <= 0) {
            return false;
        }
        sent += (size_t)n;
    }
    return true;
}

bool smtpReadLine(SmtpSession *session, char *out, size_t outMax, int *codeOut) {
    if (out == nullptr || outMax < 4) {
        return false;
    }
    size_t used = 0;
    out[0] = '\0';
    while (used + 1 < outMax) {
        unsigned char ch = 0;
        const int n = smtpRead(session, &ch, 1);
        if (n <= 0) {
            return false;
        }
        out[used++] = (char)ch;
        out[used] = '\0';
        if (used >= 2 && out[used - 2] == '\r' && out[used - 1] == '\n') {
            break;
        }
    }
    if (codeOut != nullptr) {
        *codeOut = atoi(out);
    }
    return used > 0;
}

bool smtpExpect(SmtpSession *session, int expectedCode, char *errorOut, size_t errorMax) {
    char line[256];
    int code = 0;
    if (!smtpReadLine(session, line, sizeof(line), &code)) {
        if (errorOut != nullptr && errorMax > 0) {
            strncpy(errorOut, "SMTP read failed", errorMax - 1);
            errorOut[errorMax - 1] = '\0';
        }
        return false;
    }
    // Drain multiline replies (code + '-').
    while (strlen(line) >= 4 && line[3] == '-') {
        if (!smtpReadLine(session, line, sizeof(line), &code)) {
            break;
        }
    }
    if (code != expectedCode) {
        if (errorOut != nullptr && errorMax > 0) {
            snprintf(errorOut, errorMax, "SMTP expected %d got %d: %s", expectedCode, code, line);
        }
        return false;
    }
    return true;
}

bool smtpHandshake(SmtpSession *session, const EmailSettings &email, char *errorOut, size_t errorMax) {
    char portText[16];
    snprintf(portText, sizeof(portText), "%d", email.port);
    if (mbedtls_net_connect(&session->net, email.host, portText, MBEDTLS_NET_PROTO_TCP) != 0) {
        if (errorOut != nullptr && errorMax > 0) {
            snprintf(errorOut, errorMax, "TCP connect failed to %s:%d", email.host, email.port);
        }
        return false;
    }

    const char *pers = "host-smtp";
    if (mbedtls_ctr_drbg_seed(
            &session->ctrDrbg,
            mbedtls_entropy_func,
            &session->entropy,
            reinterpret_cast<const unsigned char *>(pers),
            strlen(pers)
        )
        != 0) {
        if (errorOut != nullptr && errorMax > 0) {
            strncpy(errorOut, "DRBG seed failed", errorMax - 1);
        }
        return false;
    }

    session->useSsl = (email.encryption == EmailEncryptionSsl);
    const bool wantStartTls = (email.encryption == EmailEncryptionStartTls);

    auto setupSsl = [&]() -> bool {
        if (mbedtls_ssl_config_defaults(
                &session->conf,
                MBEDTLS_SSL_IS_CLIENT,
                MBEDTLS_SSL_TRANSPORT_STREAM,
                MBEDTLS_SSL_PRESET_DEFAULT
            )
            != 0) {
            return false;
        }
        mbedtls_ssl_conf_authmode(&session->conf, MBEDTLS_SSL_VERIFY_NONE);
        mbedtls_ssl_conf_rng(&session->conf, mbedtls_ctr_drbg_random, &session->ctrDrbg);
        if (mbedtls_ssl_setup(&session->ssl, &session->conf) != 0) {
            return false;
        }
        mbedtls_ssl_set_hostname(&session->ssl, email.host);
        mbedtls_ssl_set_bio(&session->ssl, &session->net, mbedtls_net_send, mbedtls_net_recv, nullptr);
        while (true) {
            const int rc = mbedtls_ssl_handshake(&session->ssl);
            if (rc == 0) {
                break;
            }
            if (rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE) {
                return false;
            }
        }
        session->sslReady = true;
        return true;
    };

    if (session->useSsl && !setupSsl()) {
        if (errorOut != nullptr && errorMax > 0) {
            strncpy(errorOut, "TLS handshake failed", errorMax - 1);
        }
        return false;
    }

    if (!smtpExpect(session, 220, errorOut, errorMax)) {
        return false;
    }

    const char *ehloHost = email.ehloHostname[0] != '\0' ? email.ehloHostname : "z2m-gateway";
    char ehlo[128];
    snprintf(ehlo, sizeof(ehlo), "EHLO %s\r\n", ehloHost);
    if (!smtpSendRaw(session, ehlo) || !smtpExpect(session, 250, errorOut, errorMax)) {
        return false;
    }

    if (wantStartTls) {
        if (!smtpSendRaw(session, "STARTTLS\r\n") || !smtpExpect(session, 220, errorOut, errorMax)) {
            return false;
        }
        if (!setupSsl()) {
            if (errorOut != nullptr && errorMax > 0) {
                strncpy(errorOut, "STARTTLS handshake failed", errorMax - 1);
            }
            return false;
        }
        if (!smtpSendRaw(session, ehlo) || !smtpExpect(session, 250, errorOut, errorMax)) {
            return false;
        }
    }

    if (email.authEnabled) {
        if (!smtpSendRaw(session, "AUTH LOGIN\r\n") || !smtpExpect(session, 334, errorOut, errorMax)) {
            return false;
        }
        unsigned char userB64[128];
        unsigned char passB64[128];
        size_t userLen = 0;
        size_t passLen = 0;
        if (mbedtls_base64_encode(
                userB64,
                sizeof(userB64),
                &userLen,
                reinterpret_cast<const unsigned char *>(email.username),
                strlen(email.username)
            )
            != 0
            || mbedtls_base64_encode(
                   passB64,
                   sizeof(passB64),
                   &passLen,
                   reinterpret_cast<const unsigned char *>(email.password),
                   strlen(email.password)
               )
                != 0) {
            if (errorOut != nullptr && errorMax > 0) {
                strncpy(errorOut, "AUTH base64 failed", errorMax - 1);
            }
            return false;
        }
        char userLine[160];
        char passLine[160];
        snprintf(userLine, sizeof(userLine), "%.*s\r\n", (int)userLen, userB64);
        snprintf(passLine, sizeof(passLine), "%.*s\r\n", (int)passLen, passB64);
        if (!smtpSendRaw(session, userLine) || !smtpExpect(session, 334, errorOut, errorMax)) {
            return false;
        }
        if (!smtpSendRaw(session, passLine) || !smtpExpect(session, 235, errorOut, errorMax)) {
            return false;
        }
    }
    return true;
}

bool encodeBase64Psram(const uint8_t *data, size_t length, char **outText, size_t *outLen) {
    size_t needed = 0;
    mbedtls_base64_encode(nullptr, 0, &needed, data, length);
    char *encoded = static_cast<char *>(heap_caps_malloc(needed + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (encoded == nullptr) {
        encoded = static_cast<char *>(malloc(needed + 1));
    }
    if (encoded == nullptr) {
        return false;
    }
    size_t written = 0;
    if (mbedtls_base64_encode(
            reinterpret_cast<unsigned char *>(encoded),
            needed,
            &written,
            data,
            length
        )
        != 0) {
        free(encoded);
        return false;
    }
    encoded[written] = '\0';
    *outText = encoded;
    *outLen = written;
    return true;
}

} // namespace

bool hostSmtpAccountCheck(const EmailSettings &email, char *errorOut, size_t errorMax) {
    if (!SettingsManager::isEmailConfigured(email)) {
        if (errorOut != nullptr && errorMax > 0) {
            strncpy(errorOut, "not configured", errorMax - 1);
        }
        return false;
    }
    SmtpSession session;
    smtpInit(&session);
    const bool ok = smtpHandshake(&session, email, errorOut, errorMax);
    if (ok) {
        smtpSendRaw(&session, "QUIT\r\n");
        smtpExpect(&session, 221, nullptr, 0);
        LOGGER.info("Email SMTP account check ok");
    } else {
        LOGGER.warning(errorOut != nullptr ? errorOut : "Email SMTP account check failed");
    }
    smtpFree(&session);
    return ok;
}

bool hostSmtpSendConfigured(
    const EmailSettings &email,
    const char *receivers,
    const char *subject,
    const char *body,
    const uint8_t *const *jpegData,
    const size_t *jpegSize,
    uint8_t jpegCount
) {
    char error[192] = {};
    if (!SettingsManager::isEmailConfigured(email)) {
        LOGGER.warning("Email send skipped: account not configured");
        return false;
    }
    SmtpSession session;
    smtpInit(&session);
    if (!smtpHandshake(&session, email, error, sizeof(error))) {
        LOGGER.warning(error[0] != '\0' ? error : "Email SMTP connect failed");
        smtpFree(&session);
        return false;
    }

    char mailFrom[160];
    snprintf(mailFrom, sizeof(mailFrom), "MAIL FROM:<%s>\r\n", email.fromAddress);
    if (!smtpSendRaw(&session, mailFrom) || !smtpExpect(&session, 250, error, sizeof(error))) {
        LOGGER.warning(error);
        smtpFree(&session);
        return false;
    }

    if (receivers != nullptr) {
        const char *cursor = receivers;
        while (*cursor != '\0') {
            while (*cursor == ';' || *cursor == ' ') {
                cursor++;
            }
            if (*cursor == '\0') {
                break;
            }
            const char *end = cursor;
            while (*end != '\0' && *end != ';') {
                end++;
            }
            char address[96];
            size_t len = (size_t)(end - cursor) < sizeof(address) - 1 ? (size_t)(end - cursor)
                                                                     : sizeof(address) - 1;
            memcpy(address, cursor, len);
            address[len] = '\0';
            while (len > 0 && (address[len - 1] == ' ' || address[len - 1] == '\t')) {
                len--;
                address[len] = '\0';
            }
            if (address[0] != '\0') {
                char rcpt[128];
                snprintf(rcpt, sizeof(rcpt), "RCPT TO:<%s>\r\n", address);
                if (!smtpSendRaw(&session, rcpt) || !smtpExpect(&session, 250, error, sizeof(error))) {
                    LOGGER.warning(error);
                    smtpFree(&session);
                    return false;
                }
            }
            cursor = (*end == ';') ? end + 1 : end;
        }
    }

    if (!smtpSendRaw(&session, "DATA\r\n") || !smtpExpect(&session, 354, error, sizeof(error))) {
        LOGGER.warning(error);
        smtpFree(&session);
        return false;
    }

    const bool hasJpeg = jpegCount > 0 && jpegData != nullptr && jpegSize != nullptr;
    char header[512];
    if (!hasJpeg) {
        snprintf(
            header,
            sizeof(header),
            "From: %s <%s>\r\nSubject: %s\r\nMIME-Version: 1.0\r\n"
            "Content-Type: text/plain; charset=utf-8\r\n\r\n",
            email.fromName[0] != '\0' ? email.fromName : email.fromAddress,
            email.fromAddress,
            subject != nullptr ? subject : ""
        );
        smtpSendRaw(&session, header);
        smtpSendRaw(&session, body != nullptr ? body : "");
        smtpSendRaw(&session, "\r\n.\r\n");
    } else {
        snprintf(
            header,
            sizeof(header),
            "From: %s <%s>\r\nSubject: %s\r\nMIME-Version: 1.0\r\n"
            "Content-Type: multipart/mixed; boundary=\"z2mbound\"\r\n\r\n"
            "--z2mbound\r\nContent-Type: text/plain; charset=utf-8\r\n\r\n",
            email.fromName[0] != '\0' ? email.fromName : email.fromAddress,
            email.fromAddress,
            subject != nullptr ? subject : ""
        );
        smtpSendRaw(&session, header);
        smtpSendRaw(&session, body != nullptr ? body : "");
        smtpSendRaw(&session, "\r\n");
        for (uint8_t i = 0; i < jpegCount && i < AUTOMATION_EMAIL_CAMERA_ATTACHES_MAX; ++i) {
            if (jpegData[i] == nullptr || jpegSize[i] == 0) {
                continue;
            }
            char *b64 = nullptr;
            size_t b64Len = 0;
            if (!encodeBase64Psram(jpegData[i], jpegSize[i], &b64, &b64Len)) {
                LOGGER.warning("Email JPEG base64 alloc failed");
                continue;
            }
            char partHead[160];
            snprintf(
                partHead,
                sizeof(partHead),
                "--z2mbound\r\nContent-Type: image/jpeg\r\n"
                "Content-Transfer-Encoding: base64\r\n"
                "Content-Disposition: attachment; filename=\"camera%u.jpg\"\r\n\r\n",
                (unsigned)(i + 1)
            );
            smtpSendRaw(&session, partHead);
            smtpSendRaw(&session, b64);
            smtpSendRaw(&session, "\r\n");
            free(b64);
        }
        smtpSendRaw(&session, "--z2mbound--\r\n.\r\n");
    }

    if (!smtpExpect(&session, 250, error, sizeof(error))) {
        LOGGER.warning(error);
        smtpFree(&session);
        return false;
    }
    smtpSendRaw(&session, "QUIT\r\n");
    smtpExpect(&session, 221, nullptr, 0);
    smtpFree(&session);
    LOGGER.info("Email SMTP send ok");
    return true;
}

#endif
