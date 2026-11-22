#pragma once

#include <stddef.h>
#include <stdint.h>
#include <Arduino.h>

class SceneStore;
class HistoryStore;

struct FullBackupOptions {
    bool includeWifi = true;
    bool includeHistory = true;
    bool includeSecrets = true;
};

struct FullBackupBuildResult {
    uint8_t *zipBytes = nullptr;
    size_t zipLength = 0;
    String errorText;
};

// Packs manifest + settingsJson (+ optional history + scene bakes) into one store ZIP.
FullBackupBuildResult fullBackupBuildZip(
    const char *settingsJson,
    const FullBackupOptions &options,
    SceneStore *sceneStore,
    HistoryStore *historyStore,
    const char *firmwareVersion
);

void fullBackupFreeZip(uint8_t *zipBytes);

bool fullBackupDecryptSecrets(
    const char *settingsJson,
    const char *passphrase,
    char *outWifiPassword,
    size_t outWifiPasswordBytes,
    char *outEmailPassword,
    size_t outEmailPasswordBytes,
    bool *hadSecretsOut
);

bool fullBackupEncryptSecretField(
    const char *passphrase,
    const char *plaintext,
    const uint8_t salt[16],
    String &outObjectJson
);

void fullBackupRandomSalt(uint8_t salt[16]);
uint32_t fullBackupSecretIterations();
