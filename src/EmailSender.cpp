#include "EmailSender.h"
#include "Logger.h"
#include "Defines.h"

#include <esp_heap_caps.h>
#include <string.h>

#if BOARD_ROLE_HOST
#include <ESP_Mail_Client.h>
#endif

namespace {
constexpr uint32_t kEmailOneShotStackWords = 8192;
// Below loopTask (1) and async_tcp (10) so TLS crypto cannot starve UI/MQTT.
constexpr UBaseType_t kEmailOneShotPriority = 0;
// CPU1: IDLE1 is not TWDT-watched. async_tcp is pinned to CPU0 via build flag
// so long TLS here cannot starve its WDT feed (that was the CPU1 failure mode).
constexpr BaseType_t kEmailOneShotCore = 1;

#if BOARD_ROLE_HOST
SemaphoreHandle_t gEmailSmtpMutex = nullptr;

void ensureEmailSmtpMutex() {
    if (gEmailSmtpMutex == nullptr) {
        gEmailSmtpMutex = xSemaphoreCreateMutex();
    }
}
#endif
}

EmailSender::EmailSender(SettingsManager *settingsManager)
    : settingsManager(settingsManager) {
#if BOARD_ROLE_HOST
    ensureEmailSmtpMutex();
#endif
}

bool EmailSender::isConfigured(SettingsManager *settingsManager) {
    if (settingsManager == nullptr) {
        return false;
    }
    return SettingsManager::isEmailConfigured(settingsManager->getSettings()->email);
}

char *EmailSender::duplicatePsramString(const char *source, size_t maxLength) {
    const char *text = source != nullptr ? source : "";
    size_t length = strlen(text);
    if (length > maxLength) {
        length = maxLength;
    }
    char *copy = static_cast<char *>(
        heap_caps_malloc(length + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );
    if (copy == nullptr) {
        copy = static_cast<char *>(ps_malloc(length + 1));
    }
    if (copy == nullptr) {
        return nullptr;
    }
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

void EmailSender::freeJob(EmailJob *job) {
    if (job == nullptr) {
        return;
    }
    free(job->receivers);
    free(job->theme);
    free(job->body);
    job->receivers = nullptr;
    job->theme = nullptr;
    job->body = nullptr;
    free(job);
}

bool EmailSender::enqueue(const char *receivers, const char *theme, const char *body) {
#if !BOARD_ROLE_HOST
    (void)receivers;
    (void)theme;
    (void)body;
    LOGGER.warning("Email action ignored: host-only feature");
    return false;
#else
    if (!isConfigured(settingsManager)) {
        LOGGER.warning("Email action ignored: account not configured");
        return false;
    }

    EmailJob *job = static_cast<EmailJob *>(
        heap_caps_malloc(sizeof(EmailJob), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );
    if (job == nullptr) {
        job = static_cast<EmailJob *>(ps_malloc(sizeof(EmailJob)));
    }
    if (job == nullptr) {
        LOGGER.warning("Email action ignored: out of PSRAM for send job");
        return false;
    }
    memset(job, 0, sizeof(*job));
    job->sender = this;
    job->receivers = duplicatePsramString(receivers, AUTOMATION_EMAIL_RECEIVERS_MAX - 1);
    job->theme = duplicatePsramString(theme, AUTOMATION_EMAIL_THEME_MAX - 1);
    job->body = duplicatePsramString(
        body,
        AUTOMATION_EMAIL_MESSAGE_MAX + AUTOMATION_EMAIL_PARAMS_MAX * 96
    );
    if (job->receivers == nullptr || job->theme == nullptr || job->body == nullptr) {
        freeJob(job);
        LOGGER.warning("Email action ignored: out of PSRAM for send job");
        return false;
    }

    ensureEmailSmtpMutex();
    TaskHandle_t taskHandle = nullptr;
    const BaseType_t created = xTaskCreatePinnedToCore(
        oneShotEntry,
        "emailSmtp",
        kEmailOneShotStackWords,
        job,
        kEmailOneShotPriority,
        &taskHandle,
        kEmailOneShotCore
    );
    if (created != pdPASS) {
        freeJob(job);
        LOGGER.warning("Email action ignored: one-shot SMTP task create failed");
        return false;
    }
    return true;
#endif
}

void EmailSender::oneShotEntry(void *argument) {
    EmailJob *job = static_cast<EmailJob *>(argument);
#if BOARD_ROLE_HOST
    ensureEmailSmtpMutex();
    if (gEmailSmtpMutex != nullptr) {
        xSemaphoreTake(gEmailSmtpMutex, portMAX_DELAY);
    }
    if (job != nullptr && job->sender != nullptr) {
        job->sender->sendJob(*job);
    }
    if (gEmailSmtpMutex != nullptr) {
        xSemaphoreGive(gEmailSmtpMutex);
    }
#else
    if (job != nullptr && job->sender != nullptr) {
        job->sender->sendJob(*job);
    }
#endif
    freeJob(job);
    vTaskDelay(1);
    vTaskDelete(nullptr);
}

void EmailSender::sendJob(const EmailJob &job) {
#if !BOARD_ROLE_HOST
    (void)job;
#else
    if (settingsManager == nullptr || !isConfigured(settingsManager)) {
        LOGGER.warning("Email send skipped: account not configured");
        return;
    }
    const EmailSettings &emailSettings = settingsManager->getSettings()->email;

    // SMTP client exists only for this send; destroyed when this scope ends.
    {
        SMTPSession smtp;
        Session_Config config;
        SMTP_Message message;

        config.server.host_name = emailSettings.host;
        config.server.port = emailSettings.port;
        if (emailSettings.authEnabled) {
            config.login.email = emailSettings.username;
            config.login.password = emailSettings.password;
        }
        if (emailSettings.ehloHostname[0] != '\0') {
            config.login.user_domain = emailSettings.ehloHostname;
        } else {
            config.login.user_domain = F("z2m-gateway");
        }
        config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
        config.time.gmt_offset = 0;
        config.time.day_light_offset = 0;

        if (emailSettings.encryption == EmailEncryptionNone) {
            config.secure.mode = esp_mail_secure_mode_nonsecure;
        } else {
            config.secure.mode = esp_mail_secure_mode_ssl_tls;
        }

        message.sender.name =
            emailSettings.fromName[0] != '\0' ? emailSettings.fromName : emailSettings.fromAddress;
        message.sender.email = emailSettings.fromAddress;
        if (emailSettings.replyTo[0] != '\0') {
            message.addHeader("Reply-To: " + String(emailSettings.replyTo));
        }
        message.subject = job.theme != nullptr ? job.theme : "";
        message.text.content = job.body != nullptr ? job.body : "";

        if (job.receivers != nullptr) {
            String receiversText = job.receivers;
            int start = 0;
            while (start <= (int)receiversText.length()) {
                int separator = receiversText.indexOf(';', start);
                if (separator < 0) {
                    separator = receiversText.length();
                }
                String address = receiversText.substring(start, separator);
                address.trim();
                if (address.length() > 0) {
                    message.addRecipient(address.c_str(), address.c_str());
                }
                if (separator >= (int)receiversText.length()) {
                    break;
                }
                start = separator + 1;
            }
        }

        uint16_t tcpTimeoutSec = (uint16_t)(emailSettings.connectTimeoutMs / 1000);
        const uint16_t sendTimeoutSec = (uint16_t)(emailSettings.sendTimeoutMs / 1000);
        if (sendTimeoutSec > tcpTimeoutSec) {
            tcpTimeoutSec = sendTimeoutSec;
        }
        if (tcpTimeoutSec < 15) {
            tcpTimeoutSec = 15;
        }
        smtp.setTCPTimeout(tcpTimeoutSec);
        auto logSmtpFailure = [&](const char *phase) {
            const char *encryptionName = emailEncryptionModeName(emailSettings.encryption);
            String detail;
            detail.reserve(320);
            detail += "Email SMTP ";
            detail += phase;
            detail += " failed: host=";
            detail += emailSettings.host;
            detail += " port=";
            detail += String(emailSettings.port);
            detail += " enc=";
            detail += encryptionName != nullptr ? encryptionName : "?";
            detail += " auth=";
            detail += emailSettings.authEnabled ? "on" : "off";
            if (emailSettings.authEnabled && emailSettings.username[0] != '\0') {
                detail += " user=";
                detail += emailSettings.username;
            }
            detail += " status=";
            detail += String(smtp.statusCode());
            detail += " err=";
            detail += String(smtp.errorCode());
            detail += " reason=";
            detail += smtp.errorReason();
            const String statusText = smtp.statusMessage();
            if (statusText.length() > 0) {
                detail += " server=";
                detail += statusText;
            }
            LOGGER.warning(detail);
        };
        if (!smtp.connect(&config)) {
            logSmtpFailure("connect");
            smtp.closeSession();
            return;
        }
        if (!MailClient.sendMail(&smtp, &message)) {
            logSmtpFailure("send");
            smtp.closeSession();
            return;
        }
        LOGGER.info("Email SMTP send ok");
        smtp.closeSession();
    }
#endif
}
