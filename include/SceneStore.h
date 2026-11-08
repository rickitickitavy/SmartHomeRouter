#pragma once

#include "Defines.h"
#include "PsramJsonBuffer.h"
#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

// Scene registry (host). Records and large JSON blobs live in PSRAM when available.

struct SceneRecord {
    char id[SCENE_ID_BUF];
    char name[SCENE_NAME_MAX];
    char *modelJson;
    char bakePath[SCENE_BAKE_PATH_MAX];
    char *pinsJson;
    SceneRecord *next;
};

enum SceneWriteResult {
    SceneWriteOk = 0,
    SceneWriteNotFound,
    SceneWriteFull,
    SceneWriteBadId,
    SceneWriteBadName,
    SceneWriteDuplicateId,
    SceneWriteValidation,
    SceneWriteOutOfMemory,
    SceneWriteParseError,
    SceneWriteBakeFailed
};

class SceneStore {
public:
    SceneStore();
    ~SceneStore();

    SceneStore(const SceneStore &) = delete;
    SceneStore &operator=(const SceneStore &) = delete;

    bool begin(bool persistToLittleFs);
    void requestPersist();
    void persistIfDue();

    int sceneCount() const;
    SceneRecord *first();
    const SceneRecord *first() const;
    SceneRecord *findById(const char *sceneId);
    const SceneRecord *findById(const char *sceneId) const;

    SceneWriteResult createFromJson(const char *json, SceneRecord **createdOut);
    SceneWriteResult replaceFromJson(const char *sceneId, const char *json, bool keepId = true);
    SceneWriteResult deleteById(const char *sceneId);
    SceneWriteResult replaceAllFromJson(const char *arrayJson);
    SceneWriteResult replaceAllFromJson(const String &arrayJson);
    // Insert scenes whose id is not already stored; skip duplicates. Empty ids get new ids.
    SceneWriteResult importOnlyNewFromJson(
        const char *arrayJson,
        int *addedOut = nullptr,
        int *skippedOut = nullptr
    );

    SceneWriteResult writeBakeJpeg(const char *sceneId, const uint8_t *data, size_t length);
    bool readBakeJpeg(const char *sceneId, uint8_t **dataOut, size_t *lengthOut) const;
    void deleteBakeFile(const SceneRecord *scene) const;

    String listJson() const;
    String sceneFullJson(const SceneRecord *scene) const;
    String exportJson() const;

    static const char *writeResultText(SceneWriteResult result);
    static bool isIdSafe(const char *sceneId);

    static bool psramAllowsAllocation(size_t bytes);
    static void *allocatePsram(size_t bytes);
    static void freeScene(SceneRecord *scene);
    static void freeSceneList(SceneRecord *&head);

private:
    SceneRecord *head;
    int storedCount;
    bool persistEnabled;
    bool persistPending;

    bool saveToFile() const;
    bool loadFromFile();
    void clearAll();
    SceneWriteResult parseArray(const char *arrayJson, SceneRecord **outHead, int *outCount) const;
    SceneWriteResult parseOneObject(const char *objectJson, SceneRecord **outScene) const;
    bool assignOwnedJson(char **slot, const char *jsonText, size_t maxBytes) const;
    void generateUniqueId(char *idOut) const;
    void appendSceneFull(PsramJsonBuffer &json, const SceneRecord *scene) const;
    void appendSceneListItem(PsramJsonBuffer &json, const SceneRecord *scene) const;
    void ensureBakesDir() const;
};
