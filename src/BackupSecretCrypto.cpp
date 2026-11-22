#include "BackupSecretCrypto.h"

#include <esp_random.h>
#include <mbedtls/aes.h>
#include <mbedtls/base64.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <string.h>

namespace BackupSecretCrypto {
namespace {

bool deriveKey(
    const char *passphrase,
    const uint8_t salt[16],
    uint32_t iterations,
    uint8_t keyOut[16]
) {
    if (passphrase == nullptr || passphrase[0] == '\0' || salt == nullptr || keyOut == nullptr) {
        return false;
    }
    const mbedtls_md_info_t *mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (mdInfo == nullptr) {
        return false;
    }
    (void)mdInfo;
    const int deriveErr = mbedtls_pkcs5_pbkdf2_hmac_ext(
        MBEDTLS_MD_SHA256,
        (const unsigned char *)passphrase,
        strlen(passphrase),
        salt,
        16,
        iterations,
        16,
        keyOut
    );
    return deriveErr == 0;
}

bool toBase64(const uint8_t *bytes, size_t length, String &outText) {
    size_t encodedLen = 0;
    mbedtls_base64_encode(nullptr, 0, &encodedLen, bytes, length);
    char *buffer = (char *)malloc(encodedLen + 1);
    if (buffer == nullptr) {
        return false;
    }
    size_t written = 0;
    if (mbedtls_base64_encode(
            (unsigned char *)buffer,
            encodedLen + 1,
            &written,
            bytes,
            length
        )
        != 0) {
        free(buffer);
        return false;
    }
    buffer[written] = '\0';
    outText = buffer;
    free(buffer);
    return true;
}

bool fromBase64(const char *text, uint8_t *outBytes, size_t outBytesMax, size_t *outLength) {
    if (text == nullptr || outBytes == nullptr || outLength == nullptr) {
        return false;
    }
    size_t written = 0;
    if (mbedtls_base64_decode(outBytes, outBytesMax, &written, (const unsigned char *)text, strlen(text))
        != 0) {
        return false;
    }
    *outLength = written;
    return true;
}

bool extractJsonStringField(const char *objectJson, const char *key, String &outValue) {
    outValue = "";
    if (objectJson == nullptr || key == nullptr) {
        return false;
    }
    String needle = String("\"") + key + "\":\"";
    const char *found = strstr(objectJson, needle.c_str());
    if (found == nullptr) {
        return false;
    }
    found += needle.length();
    String value;
    while (*found != '\0' && *found != '"') {
        if (*found == '\\' && found[1] != '\0') {
            value += found[1];
            found += 2;
            continue;
        }
        value += *found;
        found++;
    }
    outValue = value;
    return true;
}

}  // namespace

void randomSalt(uint8_t salt[16]) {
    esp_fill_random(salt, 16);
}

bool encryptToJsonObject(
    const char *passphrase,
    const char *plaintext,
    const uint8_t salt[16],
    uint32_t iterations,
    String &outObjectJson
) {
    outObjectJson = "";
    if (plaintext == nullptr) {
        plaintext = "";
    }
    uint8_t key[16];
    if (!deriveKey(passphrase, salt, iterations, key)) {
        return false;
    }
    uint8_t iv[16];
    esp_fill_random(iv, sizeof(iv));
    const size_t plainLen = strlen(plaintext);
    const size_t paddedLen = ((plainLen / 16) + 1) * 16;
    uint8_t *padded = (uint8_t *)malloc(paddedLen);
    uint8_t *cipher = (uint8_t *)malloc(paddedLen);
    if (padded == nullptr || cipher == nullptr) {
        free(padded);
        free(cipher);
        return false;
    }
    memcpy(padded, plaintext, plainLen);
    const uint8_t padValue = (uint8_t)(paddedLen - plainLen);
    memset(padded + plainLen, padValue, paddedLen - plainLen);
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_enc(&aes, key, 128) != 0) {
        mbedtls_aes_free(&aes);
        free(padded);
        free(cipher);
        return false;
    }
    uint8_t ivWork[16];
    memcpy(ivWork, iv, 16);
    if (mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, paddedLen, ivWork, padded, cipher) != 0) {
        mbedtls_aes_free(&aes);
        free(padded);
        free(cipher);
        return false;
    }
    mbedtls_aes_free(&aes);
    free(padded);
    String ivText;
    String ctText;
    const bool ok = toBase64(iv, 16, ivText) && toBase64(cipher, paddedLen, ctText);
    free(cipher);
    if (!ok) {
        return false;
    }
    outObjectJson = "{\"iv\":\"";
    outObjectJson += ivText;
    outObjectJson += "\",\"ct\":\"";
    outObjectJson += ctText;
    outObjectJson += "\"}";
    return true;
}

bool decryptFromJsonObject(
    const char *passphrase,
    const char *objectJson,
    const uint8_t salt[16],
    uint32_t iterations,
    char *outPlaintext,
    size_t outPlaintextBytes
) {
    if (outPlaintext == nullptr || outPlaintextBytes == 0) {
        return false;
    }
    outPlaintext[0] = '\0';
    String ivText;
    String ctText;
    if (!extractJsonStringField(objectJson, "iv", ivText)
        || !extractJsonStringField(objectJson, "ct", ctText)) {
        return false;
    }
    uint8_t key[16];
    if (!deriveKey(passphrase, salt, iterations, key)) {
        return false;
    }
    uint8_t iv[16];
    size_t ivLen = 0;
    if (!fromBase64(ivText.c_str(), iv, sizeof(iv), &ivLen) || ivLen != 16) {
        return false;
    }
    size_t ctMax = ctText.length();
    uint8_t *cipher = (uint8_t *)malloc(ctMax);
    uint8_t *plain = (uint8_t *)malloc(ctMax);
    if (cipher == nullptr || plain == nullptr) {
        free(cipher);
        free(plain);
        return false;
    }
    size_t ctLen = 0;
    if (!fromBase64(ctText.c_str(), cipher, ctMax, &ctLen) || ctLen == 0 || (ctLen % 16) != 0) {
        free(cipher);
        free(plain);
        return false;
    }
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_dec(&aes, key, 128) != 0) {
        mbedtls_aes_free(&aes);
        free(cipher);
        free(plain);
        return false;
    }
    uint8_t ivWork[16];
    memcpy(ivWork, iv, 16);
    if (mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, ctLen, ivWork, cipher, plain) != 0) {
        mbedtls_aes_free(&aes);
        free(cipher);
        free(plain);
        return false;
    }
    mbedtls_aes_free(&aes);
    free(cipher);
    const uint8_t padValue = plain[ctLen - 1];
    if (padValue == 0 || padValue > 16 || padValue > ctLen) {
        free(plain);
        return false;
    }
    for (size_t index = ctLen - padValue; index < ctLen; index++) {
        if (plain[index] != padValue) {
            free(plain);
            return false;
        }
    }
    const size_t plainLen = ctLen - padValue;
    if (plainLen + 1 > outPlaintextBytes) {
        free(plain);
        return false;
    }
    memcpy(outPlaintext, plain, plainLen);
    outPlaintext[plainLen] = '\0';
    free(plain);
    return true;
}

}  // namespace BackupSecretCrypto
