#pragma once

#include "PsramJsonBuffer.h"
#include "StoreZip.h"

#include <Arduino.h>
#include <FS.h>
#include <stddef.h>
#include <stdint.h>

class SceneStore;
class HistoryStore;
struct SceneRecord;

struct FullBackupOptions {
    bool includeWifi = true;
    bool includeHistory = true;
    bool includeSecrets = true;
};

// Streams a Full backup ZIP into caller buffers (4–8 KiB). Does not retain the archive.
class FullBackupStreamSession {
public:
    static constexpr size_t kWorkBufferMin = 4096;
    static constexpr size_t kWorkBufferMax = 8192;

    FullBackupStreamSession();
    ~FullBackupStreamSession();

    FullBackupStreamSession(const FullBackupStreamSession &) = delete;
    FullBackupStreamSession &operator=(const FullBackupStreamSession &) = delete;

    bool begin(
        PsramJsonBuffer settingsJson,
        const FullBackupOptions &options,
        SceneStore *sceneStore,
        HistoryStore *historyStore,
        const char *firmwareVersion
    );

    // Fill out[0..outMax). Returns bytes written; 0 means finished (or failed — check ok()).
    size_t fill(uint8_t *out, size_t outMax);
    bool ok() const { return !failFlag; }
    bool finished() const { return doneFlag; }
    const char *errorText() const { return failFlag ? failMessage.c_str() : ""; }

private:
    enum Stage {
        StageManifest = 0,
        StageSettings,
        StageBakes,
        StageHistory,
        StageCentral,
        StageComplete
    };

    enum StagePhase {
        StagePhaseBegin = 0,
        StagePhaseHeader,
        StagePhaseBody,
        StagePhaseEndFile,
        StagePhaseDesc
    };

    StoreZipStreamEncoder encoder;
    PsramJsonBuffer settingsJson;
    String manifestJson;
    FullBackupOptions options;
    SceneStore *sceneStore = nullptr;
    HistoryStore *historyStore = nullptr;
    const SceneRecord *bakeScene = nullptr;
    File bakeFile;
    bool bakeFileOpen = false;
    bool bakeFileFromSd = false;
    char bakeMemberPath[96];

    const uint8_t *bodyBytes = nullptr;
    size_t bodyLength = 0;
    size_t bodyOffset = 0;
    bool bodyFromFile = false;

    uint8_t *historyBytes = nullptr;
    size_t historyLength = 0;

    Stage stage = StageManifest;
    StagePhase stagePhase = StagePhaseBegin;
    bool failFlag = false;
    bool doneFlag = false;
    String failMessage;

    void fail(const char *message);
    void closeBakeFile();
    void freeHistory();
    bool beginCurrentMember();
    bool advanceAfterMember();
    size_t emitPending(uint8_t *out, size_t outMax);
    size_t emitBody(uint8_t *out, size_t outMax);
};

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
