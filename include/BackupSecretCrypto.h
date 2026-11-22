#pragma once

#include <stddef.h>
#include <stdint.h>
#include <Arduino.h>

// Passphrase → PBKDF2-SHA256 → AES-128-CBC for important backup secrets only.
namespace BackupSecretCrypto {

bool encryptToJsonObject(
    const char *passphrase,
    const char *plaintext,
    const uint8_t salt[16],
    uint32_t iterations,
    String &outObjectJson
);

bool decryptFromJsonObject(
    const char *passphrase,
    const char *objectJson,
    const uint8_t salt[16],
    uint32_t iterations,
    char *outPlaintext,
    size_t outPlaintextBytes
);

void randomSalt(uint8_t salt[16]);

constexpr uint32_t kDefaultIterations = 10000;

}  // namespace BackupSecretCrypto
