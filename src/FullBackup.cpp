#include "FullBackup.h"

#include "BackupSecretCrypto.h"
#include "HistoryStore.h"
#include "JsonField.h"
#include "SceneStore.h"
#include "StoreZip.h"

#include <LittleFS.h>
#include <stdio.h>
#include <string.h>
#include <esp_heap_caps.h>

namespace {

bool bytesToHex(const uint8_t *bytes, size_t length, char *outText, size_t outBytes) {
    if (outText == nullptr || outBytes < length * 2 + 1) {
        return false;
    }
    static const char *kHex = "0123456789abcdef";
    for (size_t index = 0; index < length; index++) {
        outText[index * 2] = kHex[(bytes[index] >> 4) & 0x0f];
        outText[index * 2 + 1] = kHex[bytes[index] & 0x0f];
    }
    outText[length * 2] = '\0';
    return true;
}

bool hexToBytes(const char *hex, uint8_t *outBytes, size_t outLength) {
    if (hex == nullptr || outBytes == nullptr) {
        return false;
    }
    if (strlen(hex) != outLength * 2) {
        return false;
    }
    auto nibble = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') {
            return ch - '0';
        }
        if (ch >= 'a' && ch <= 'f') {
            return ch - 'a' + 10;
        }
        if (ch >= 'A' && ch <= 'F') {
            return ch - 'A' + 10;
        }
        return -1;
    };
    for (size_t index = 0; index < outLength; index++) {
        const int high = nibble(hex[index * 2]);
        const int low = nibble(hex[index * 2 + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        outBytes[index] = (uint8_t)((high << 4) | low);
    }
    return true;
}

}  // namespace

void fullBackupFreeZip(uint8_t *zipBytes) {
    if (zipBytes != nullptr) {
        heap_caps_free(zipBytes);
    }
}

void fullBackupRandomSalt(uint8_t salt[16]) {
    BackupSecretCrypto::randomSalt(salt);
}

uint32_t fullBackupSecretIterations() {
    return BackupSecretCrypto::kDefaultIterations;
}

bool fullBackupEncryptSecretField(
    const char *passphrase,
    const char *plaintext,
    const uint8_t salt[16],
    String &outObjectJson
) {
    return BackupSecretCrypto::encryptToJsonObject(
        passphrase,
        plaintext,
        salt,
        BackupSecretCrypto::kDefaultIterations,
        outObjectJson
    );
}

FullBackupBuildResult fullBackupBuildZip(
    const char *settingsJson,
    const FullBackupOptions &options,
    SceneStore *sceneStore,
    HistoryStore *historyStore,
    const char *firmwareVersion
) {
    FullBackupBuildResult result;
    if (settingsJson == nullptr) {
        result.errorText = "Missing settings JSON";
        return result;
    }
    String manifest = "{\"format\":1,\"includeWifi\":";
    manifest += options.includeWifi ? "true" : "false";
    manifest += ",\"includeHistory\":";
    manifest += options.includeHistory ? "true" : "false";
    manifest += ",\"includeSecrets\":";
    manifest += options.includeSecrets ? "true" : "false";
    manifest += ",\"firmware\":\"";
    manifest += firmwareVersion != nullptr ? firmwareVersion : "";
    manifest += "\"}";

    StoreZipWriter zip;
    if (!zip.addFile("manifest.json", (const uint8_t *)manifest.c_str(), manifest.length())
        || !zip.addFile(
            "settings.json",
            (const uint8_t *)settingsJson,
            strlen(settingsJson)
        )) {
        result.errorText = "Failed to pack settings";
        return result;
    }

    if (sceneStore != nullptr) {
        for (const SceneRecord *scene = sceneStore->first(); scene != nullptr;
             scene = scene->next) {
            if (scene->id[0] == '\0') {
                continue;
            }
            uint8_t *bakeBytes = nullptr;
            size_t bakeLen = 0;
            if (!sceneStore->readBakeJpeg(scene->id, &bakeBytes, &bakeLen) || bakeBytes == nullptr) {
                continue;
            }
            char bakeMember[96];
            snprintf(bakeMember, sizeof(bakeMember), "scenes/bakes/%s.jpg", scene->id);
            const bool added = zip.addFile(bakeMember, bakeBytes, bakeLen);
            free(bakeBytes);
            if (!added) {
                result.errorText = "Failed to pack scene bake";
                return result;
            }
        }
    }

    if (options.includeHistory && historyStore != nullptr) {
        uint8_t *historyBytes = nullptr;
        size_t historyLen = 0;
        if (historyStore->exportSnapshotToHeap(&historyBytes, &historyLen)
            && historyBytes != nullptr) {
            if (!zip.addFile("history/snapshot.bin", historyBytes, historyLen)) {
                free(historyBytes);
                result.errorText = "Failed to pack history";
                return result;
            }
            free(historyBytes);
        }
    }

    if (!zip.finish(&result.zipBytes, &result.zipLength) || result.zipBytes == nullptr) {
        result.errorText = "Failed to finish ZIP";
        result.zipBytes = nullptr;
        result.zipLength = 0;
        return result;
    }
    return result;
}

bool fullBackupDecryptSecrets(
    const char *settingsJson,
    const char *passphrase,
    char *outWifiPassword,
    size_t outWifiPasswordBytes,
    char *outEmailPassword,
    size_t outEmailPasswordBytes,
    bool *hadSecretsOut
) {
    if (outWifiPassword != nullptr && outWifiPasswordBytes > 0) {
        outWifiPassword[0] = '\0';
    }
    if (outEmailPassword != nullptr && outEmailPasswordBytes > 0) {
        outEmailPassword[0] = '\0';
    }
    if (hadSecretsOut != nullptr) {
        *hadSecretsOut = false;
    }
    if (settingsJson == nullptr) {
        return true;
    }
    String secretsSlice;
    if (!extractJsonKeyedSlice(settingsJson, "secrets", '{', secretsSlice)) {
        return true;
    }
    if (hadSecretsOut != nullptr) {
        *hadSecretsOut = true;
    }
    if (passphrase == nullptr || passphrase[0] == '\0') {
        return false;
    }
    int iterations = (int)BackupSecretCrypto::kDefaultIterations;
    extractJsonInt(secretsSlice.c_str(), "iter", iterations);
    String saltHex;
    if (!extractJsonString(secretsSlice.c_str(), "salt", saltHex)) {
        return false;
    }
    uint8_t salt[16];
    if (!hexToBytes(saltHex.c_str(), salt, sizeof(salt))) {
        return false;
    }
    String emailObject;
    if (extractJsonKeyedSlice(secretsSlice.c_str(), "emailPassword", '{', emailObject)
        && outEmailPassword != nullptr
        && outEmailPasswordBytes > 0) {
        if (!BackupSecretCrypto::decryptFromJsonObject(
                passphrase,
                emailObject.c_str(),
                salt,
                (uint32_t)iterations,
                outEmailPassword,
                outEmailPasswordBytes
            )) {
            return false;
        }
    }
    String wifiObject;
    if (extractJsonKeyedSlice(secretsSlice.c_str(), "wifiPassword", '{', wifiObject)
        && outWifiPassword != nullptr
        && outWifiPasswordBytes > 0) {
        if (!BackupSecretCrypto::decryptFromJsonObject(
                passphrase,
                wifiObject.c_str(),
                salt,
                (uint32_t)iterations,
                outWifiPassword,
                outWifiPasswordBytes
            )) {
            return false;
        }
    }
    return true;
}
