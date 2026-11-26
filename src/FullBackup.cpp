#include "FullBackup.h"

#include "BackupSecretCrypto.h"
#include "HistoryStore.h"
#include "JsonField.h"
#include "SceneStore.h"

#include <LittleFS.h>
#include <SD_MMC.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

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

FullBackupStreamSession::FullBackupStreamSession() {
    bakeMemberPath[0] = '\0';
}

FullBackupStreamSession::~FullBackupStreamSession() {
    closeBakeFile();
    freeHistory();
}

void FullBackupStreamSession::fail(const char *message) {
    failFlag = true;
    doneFlag = true;
    stage = StageComplete;
    failMessage = message != nullptr ? message : "Full backup failed";
    closeBakeFile();
    freeHistory();
}

void FullBackupStreamSession::closeBakeFile() {
    if (bakeFileOpen) {
        bakeFile.close();
        bakeFileOpen = false;
    }
    if (bakeFileFromSd) {
        if (sceneStore != nullptr) {
            sceneStore->endSdImageSession();
        }
        bakeFileFromSd = false;
    }
}

void FullBackupStreamSession::freeHistory() {
    if (historyBytes != nullptr) {
        free(historyBytes);
        historyBytes = nullptr;
    }
    historyLength = 0;
}

bool FullBackupStreamSession::begin(
    PsramJsonBuffer settings,
    const FullBackupOptions &backupOptions,
    SceneStore *scenes,
    HistoryStore *history,
    const char *firmwareVersion
) {
    settingsJson = static_cast<PsramJsonBuffer &&>(settings);
    options = backupOptions;
    sceneStore = scenes;
    historyStore = history;
    failFlag = false;
    doneFlag = false;
    failMessage = "";
    closeBakeFile();
    freeHistory();
    bakeScene = nullptr;
    bodyBytes = nullptr;
    bodyLength = 0;
    bodyOffset = 0;
    bodyFromFile = false;
    bakeFileFromSd = false;
    bakeMemberPath[0] = '\0';

    if (!settingsJson.ok() || settingsJson.length() == 0) {
        fail("Missing settings JSON");
        return false;
    }

    manifestJson = "{\"format\":1,\"includeWifi\":";
    manifestJson += options.includeWifi ? "true" : "false";
    manifestJson += ",\"includeHistory\":";
    manifestJson += options.includeHistory ? "true" : "false";
    manifestJson += ",\"includeSecrets\":";
    manifestJson += options.includeSecrets ? "true" : "false";
    manifestJson += ",\"firmware\":\"";
    manifestJson += firmwareVersion != nullptr ? firmwareVersion : "";
    manifestJson += "\"}";

    stage = StageManifest;
    stagePhase = StagePhaseBegin;
    return true;
}

size_t FullBackupStreamSession::emitPending(uint8_t *out, size_t outMax) {
    size_t written = 0;
    encoder.emitPending(out, outMax, &written);
    if (encoder.failed()) {
        fail("ZIP encode failed");
        return 0;
    }
    return written;
}

bool FullBackupStreamSession::beginCurrentMember() {
    bodyBytes = nullptr;
    bodyLength = 0;
    bodyOffset = 0;
    bodyFromFile = false;
    closeBakeFile();

    if (stage == StageManifest) {
        if (!encoder.startFile("manifest.json")) {
            fail("Failed to start manifest member");
            return false;
        }
        bodyBytes = (const uint8_t *)manifestJson.c_str();
        bodyLength = manifestJson.length();
        return true;
    }
    if (stage == StageSettings) {
        if (!encoder.startFile("settings.json")) {
            fail("Failed to start settings member");
            return false;
        }
        bodyBytes = (const uint8_t *)settingsJson.c_str();
        bodyLength = settingsJson.length();
        return true;
    }
    if (stage == StageBakes) {
        while (bakeScene != nullptr) {
            if (bakeScene->id[0] == '\0' || bakeScene->bakePath[0] == '\0') {
                bakeScene = bakeScene->next;
                continue;
            }
            bakeFileFromSd = false;
            if (SceneStore::isCustomKind(bakeScene)) {
                if (sceneStore == nullptr || !sceneStore->beginSdImageSession()) {
                    bakeScene = bakeScene->next;
                    continue;
                }
                if (!SD_MMC.exists(bakeScene->bakePath)) {
                    sceneStore->endSdImageSession();
                    bakeScene = bakeScene->next;
                    continue;
                }
                bakeFile = SD_MMC.open(bakeScene->bakePath, "r");
                if (!bakeFile) {
                    sceneStore->endSdImageSession();
                    bakeScene = bakeScene->next;
                    continue;
                }
                bakeFileOpen = true;
                bakeFileFromSd = true;
                const char *ext = "jpg";
                if (strstr(bakeScene->bakePath, ".png") != nullptr) {
                    ext = "png";
                }
                snprintf(
                    bakeMemberPath,
                    sizeof(bakeMemberPath),
                    "scenes/custom/%s.%s",
                    bakeScene->id,
                    ext
                );
            } else {
                if (!LittleFS.exists(bakeScene->bakePath)) {
                    bakeScene = bakeScene->next;
                    continue;
                }
                bakeFile = LittleFS.open(bakeScene->bakePath, "r");
                if (!bakeFile) {
                    bakeScene = bakeScene->next;
                    continue;
                }
                bakeFileOpen = true;
                snprintf(bakeMemberPath, sizeof(bakeMemberPath), "scenes/bakes/%s.jpg", bakeScene->id);
            }
            if (!encoder.startFile(bakeMemberPath)) {
                closeBakeFile();
                fail("Failed to start bake member");
                return false;
            }
            bodyFromFile = true;
            return true;
        }
        // No more bakes — advance without starting a member.
        return false;
    }
    if (stage == StageHistory) {
        if (!options.includeHistory || historyStore == nullptr) {
            return false;
        }
        uint8_t *bytes = nullptr;
        size_t length = 0;
        if (!historyStore->exportSnapshotToHeap(&bytes, &length) || bytes == nullptr || length == 0) {
            // No samples — skip history member.
            if (bytes != nullptr) {
                free(bytes);
            }
            return false;
        }
        historyBytes = bytes;
        historyLength = length;
        if (!encoder.startFile("history/snapshot.bin")) {
            freeHistory();
            fail("Failed to start history member");
            return false;
        }
        bodyBytes = historyBytes;
        bodyLength = historyLength;
        return true;
    }
    return false;
}

bool FullBackupStreamSession::advanceAfterMember() {
    if (stage == StageManifest) {
        stage = StageSettings;
        stagePhase = StagePhaseBegin;
        return true;
    }
    if (stage == StageSettings) {
        stage = StageBakes;
        bakeScene = sceneStore != nullptr ? sceneStore->first() : nullptr;
        stagePhase = StagePhaseBegin;
        return true;
    }
    if (stage == StageBakes) {
        if (bakeScene != nullptr) {
            bakeScene = bakeScene->next;
        }
        stagePhase = StagePhaseBegin;
        return true;
    }
    if (stage == StageHistory) {
        freeHistory();
        stage = StageCentral;
        stagePhase = StagePhaseBegin;
        return true;
    }
    return false;
}

size_t FullBackupStreamSession::emitBody(uint8_t *out, size_t outMax) {
    if (bodyFromFile) {
        if (!bakeFileOpen) {
            fail("Bake file closed");
            return 0;
        }
        const size_t got = bakeFile.read(out, outMax);
        if (got > 0) {
            encoder.noteFileBytes(out, got);
        }
        if (got < outMax) {
            stagePhase = StagePhaseEndFile;
        }
        return got;
    }
    if (bodyBytes == nullptr || bodyOffset >= bodyLength) {
        stagePhase = StagePhaseEndFile;
        return 0;
    }
    size_t written = 0;
    const size_t consumed = encoder.writeFileData(
        bodyBytes + bodyOffset,
        bodyLength - bodyOffset,
        out,
        outMax,
        &written
    );
    bodyOffset += consumed;
    if (encoder.failed()) {
        fail("ZIP encode failed");
        return 0;
    }
    if (bodyOffset >= bodyLength) {
        stagePhase = StagePhaseEndFile;
    }
    return written;
}

size_t FullBackupStreamSession::fill(uint8_t *out, size_t outMax) {
    if (out == nullptr || outMax == 0 || doneFlag) {
        return 0;
    }

    size_t totalWritten = 0;
    // Bound iterations so a stuck phase cannot spin forever inside one HTTP callback.
    for (int guard = 0; guard < 64 && totalWritten < outMax && !doneFlag && !failFlag; guard++) {
        uint8_t *cursor = out + totalWritten;
        const size_t room = outMax - totalWritten;
        size_t written = 0;

        if (stage == StageCentral) {
            if (stagePhase == StagePhaseBegin) {
                if (!encoder.finishArchive()) {
                    fail("Failed to finish ZIP");
                    break;
                }
                stagePhase = StagePhaseDesc;
            }
            written = emitPending(cursor, room);
            totalWritten += written;
            if (failFlag) {
                break;
            }
            if (!encoder.hasPending()) {
                doneFlag = true;
                stage = StageComplete;
                closeBakeFile();
                freeHistory();
            }
            if (written == 0) {
                break;
            }
            continue;
        }

        if (stage == StageComplete) {
            doneFlag = true;
            break;
        }

        if (stagePhase == StagePhaseBegin) {
            if (stage == StageBakes) {
                if (beginCurrentMember()) {
                    stagePhase = StagePhaseHeader;
                } else {
                    stage = StageHistory;
                    stagePhase = StagePhaseBegin;
                }
                continue;
            }
            if (stage == StageHistory) {
                if (beginCurrentMember()) {
                    stagePhase = StagePhaseHeader;
                } else {
                    stage = StageCentral;
                    stagePhase = StagePhaseBegin;
                }
                continue;
            }
            if (!beginCurrentMember()) {
                fail("Failed to start ZIP member");
                break;
            }
            stagePhase = StagePhaseHeader;
            continue;
        }

        if (stagePhase == StagePhaseHeader) {
            written = emitPending(cursor, room);
            totalWritten += written;
            if (failFlag) {
                break;
            }
            if (!encoder.hasPending()) {
                stagePhase = StagePhaseBody;
            }
            if (written == 0 && encoder.hasPending()) {
                break;
            }
            continue;
        }

        if (stagePhase == StagePhaseBody) {
            if (encoder.hasPending()) {
                written = emitPending(cursor, room);
                totalWritten += written;
                continue;
            }
            written = emitBody(cursor, room);
            totalWritten += written;
            if (failFlag) {
                break;
            }
            if (written == 0 && stagePhase == StagePhaseBody) {
                // Empty body member.
                stagePhase = StagePhaseEndFile;
            }
            continue;
        }

        if (stagePhase == StagePhaseEndFile) {
            closeBakeFile();
            if (!encoder.finishFile()) {
                fail("Failed to finish ZIP member");
                break;
            }
            if (stage == StageHistory) {
                freeHistory();
                bodyBytes = nullptr;
            }
            stagePhase = StagePhaseDesc;
            continue;
        }

        if (stagePhase == StagePhaseDesc) {
            written = emitPending(cursor, room);
            totalWritten += written;
            if (failFlag) {
                break;
            }
            if (!encoder.hasPending()) {
                advanceAfterMember();
            }
            if (written == 0 && encoder.hasPending()) {
                break;
            }
            continue;
        }
    }

    return totalWritten;
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
