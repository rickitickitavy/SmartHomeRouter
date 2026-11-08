#include "SceneStore.h"
#include "JsonField.h"
#include "Logger.h"

#include <LittleFS.h>
#include <esp_heap_caps.h>
#include <esp_random.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

bool isJsonSpace(char character) {
    return character == ' ' || character == '\t' || character == '\n' || character == '\r';
}

const char *findJsonObjectEnd(const char *start) {
    if (start == nullptr || *start != '{') {
        return nullptr;
    }
    int depth = 0;
    const char *cursor = start;
    bool inString = false;
    bool escape = false;
    while (*cursor != '\0') {
        const char ch = *cursor;
        if (inString) {
            if (escape) {
                escape = false;
            } else if (ch == '\\') {
                escape = true;
            } else if (ch == '"') {
                inString = false;
            }
            cursor++;
            continue;
        }
        if (ch == '"') {
            inString = true;
            cursor++;
            continue;
        }
        if (ch == '{') {
            depth++;
        } else if (ch == '}') {
            depth--;
            if (depth == 0) {
                return cursor;
            }
        }
        cursor++;
    }
    return nullptr;
}

bool assignJsonSlice(const char *start, const char *endInclusive, String &out) {
    if (start == nullptr || endInclusive == nullptr || endInclusive < start) {
        return false;
    }
    const size_t length = (size_t)(endInclusive - start) + 1;
    out = "";
    if (!out.reserve(length + 1)) {
        return false;
    }
    for (size_t index = 0; index < length; index++) {
        out += start[index];
    }
    return true;
}

void copyBounded(char *destination, size_t destinationSize, const char *source) {
    if (destination == nullptr || destinationSize == 0) {
        return;
    }
    destination[0] = '\0';
    if (source == nullptr) {
        return;
    }
    strncpy(destination, source, destinationSize - 1);
    destination[destinationSize - 1] = '\0';
}

void appendJsonEscaped(PsramJsonBuffer &json, const char *text, size_t maxChars) {
    if (text == nullptr) {
        return;
    }
    size_t count = 0;
    while (text[count] != '\0' && count < maxChars) {
        const char ch = text[count];
        if (ch == '"' || ch == '\\') {
            json += '\\';
            json += ch;
        } else if (ch == '\n') {
            json += "\\n";
        } else if (ch == '\r') {
            json += "\\r";
        } else if (ch == '\t') {
            json += "\\t";
        } else if ((uint8_t)ch < 0x20) {
            char escape[7];
            snprintf(escape, sizeof(escape), "\\u%04x", (unsigned)(uint8_t)ch);
            json += escape;
        } else {
            json += ch;
        }
        count++;
    }
}

}  // namespace

SceneStore::SceneStore()
    : head(nullptr),
      storedCount(0),
      persistEnabled(false),
      persistPending(false) {}

SceneStore::~SceneStore() {
    clearAll();
}

bool SceneStore::psramAllowsAllocation(size_t bytes) {
    if (bytes == 0) {
        return true;
    }
    const size_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return freeHeap >= bytes + 4096;
}

void *SceneStore::allocatePsram(size_t bytes) {
    if (bytes == 0 || !psramAllowsAllocation(bytes)) {
        return nullptr;
    }
    return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void SceneStore::freeScene(SceneRecord *scene) {
    if (scene == nullptr) {
        return;
    }
    free(scene->modelJson);
    free(scene->pinsJson);
    free(scene);
}

void SceneStore::freeSceneList(SceneRecord *&listHead) {
    while (listHead != nullptr) {
        SceneRecord *next = listHead->next;
        freeScene(listHead);
        listHead = next;
    }
    listHead = nullptr;
}

void SceneStore::clearAll() {
    freeSceneList(head);
    storedCount = 0;
}

bool SceneStore::isIdSafe(const char *sceneId) {
    if (sceneId == nullptr || sceneId[0] == '\0') {
        return false;
    }
    const size_t length = strlen(sceneId);
    if (length >= SCENE_ID_BUF) {
        return false;
    }
    for (size_t index = 0; index < length; index++) {
        const char ch = sceneId[index];
        const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')
            || ch == '-' || ch == '_';
        if (!ok) {
            return false;
        }
    }
    return true;
}

const char *SceneStore::writeResultText(SceneWriteResult result) {
    switch (result) {
        case SceneWriteOk:
            return "ok";
        case SceneWriteNotFound:
            return "not found";
        case SceneWriteFull:
            return "full";
        case SceneWriteBadId:
            return "bad id";
        case SceneWriteBadName:
            return "bad name";
        case SceneWriteDuplicateId:
            return "duplicate id";
        case SceneWriteValidation:
            return "validation";
        case SceneWriteOutOfMemory:
            return "out of memory";
        case SceneWriteParseError:
            return "parse error";
        case SceneWriteBakeFailed:
            return "bake failed";
        default:
            return "error";
    }
}

bool SceneStore::begin(bool persistToLittleFs) {
    persistEnabled = persistToLittleFs;
    ensureBakesDir();
    if (!persistEnabled) {
        return true;
    }
    if (loadFromFile()) {
        LOGGER.info("Loaded " + String(storedCount) + " scene(s)");
        return true;
    }
    clearAll();
    return true;
}

void SceneStore::requestPersist() {
    persistPending = true;
}

void SceneStore::persistIfDue() {
    if (!persistPending) {
        return;
    }
    if (!persistEnabled) {
        persistPending = false;
        return;
    }
    persistPending = false;
    if (!saveToFile()) {
        LOGGER.error("Scene store write failed");
        persistPending = true;
        return;
    }
    LOGGER.info("Scene store saved " + String(storedCount) + " scene(s)");
}

void SceneStore::ensureBakesDir() const {
    if (!LittleFS.exists(SCENE_BAKES_DIR)) {
        LittleFS.mkdir(SCENE_BAKES_DIR);
    }
}

int SceneStore::sceneCount() const {
    return storedCount;
}

SceneRecord *SceneStore::first() {
    return head;
}

const SceneRecord *SceneStore::first() const {
    return head;
}

SceneRecord *SceneStore::findById(const char *sceneId) {
    return const_cast<SceneRecord *>(static_cast<const SceneStore *>(this)->findById(sceneId));
}

const SceneRecord *SceneStore::findById(const char *sceneId) const {
    if (sceneId == nullptr || sceneId[0] == '\0') {
        return nullptr;
    }
    for (const SceneRecord *scene = head; scene != nullptr; scene = scene->next) {
        if (strcmp(scene->id, sceneId) == 0) {
            return scene;
        }
    }
    return nullptr;
}

bool SceneStore::assignOwnedJson(char **slot, const char *jsonText, size_t maxBytes) const {
    if (slot == nullptr) {
        return false;
    }
    free(*slot);
    *slot = nullptr;
    if (jsonText == nullptr) {
        return true;
    }
    const size_t length = strlen(jsonText);
    if (length > maxBytes) {
        return false;
    }
    char *copy = (char *)allocatePsram(length + 1);
    if (copy == nullptr) {
        return false;
    }
    memcpy(copy, jsonText, length);
    copy[length] = '\0';
    *slot = copy;
    return true;
}

void SceneStore::generateUniqueId(char *idOut) const {
    if (idOut == nullptr) {
        return;
    }
    for (int attempt = 0; attempt < 32; attempt++) {
        const uint32_t value = esp_random();
        snprintf(idOut, SCENE_ID_BUF, "scn-%08x", (unsigned)value);
        if (findById(idOut) == nullptr) {
            return;
        }
    }
    snprintf(idOut, SCENE_ID_BUF, "scn-%08x", (unsigned)millis());
}

SceneWriteResult SceneStore::parseOneObject(const char *objectJson, SceneRecord **outScene) const {
    if (objectJson == nullptr || outScene == nullptr) {
        return SceneWriteParseError;
    }
    *outScene = nullptr;
    SceneRecord *scene = (SceneRecord *)allocatePsram(sizeof(SceneRecord));
    if (scene == nullptr) {
        return SceneWriteOutOfMemory;
    }
    memset(scene, 0, sizeof(*scene));
    String idText;
    String nameText;
    String bakePathText;
    String modelJson;
    String pinsJson;
    extractJsonString(objectJson, "id", idText);
    extractJsonString(objectJson, "name", nameText);
    extractJsonString(objectJson, "bakePath", bakePathText);
    extractJsonKeyedSlice(objectJson, "sceneModel", '{', modelJson);
    if (!extractJsonKeyedSlice(objectJson, "pins", '[', pinsJson)) {
        pinsJson = "[]";
    }
    if (nameText.length() == 0 || nameText.length() >= SCENE_NAME_MAX) {
        freeScene(scene);
        return SceneWriteBadName;
    }
    if (idText.length() > 0) {
        if (!isIdSafe(idText.c_str())) {
            freeScene(scene);
            return SceneWriteBadId;
        }
        copyBounded(scene->id, sizeof(scene->id), idText.c_str());
    }
    copyBounded(scene->name, sizeof(scene->name), nameText.c_str());
    if (bakePathText.length() > 0 && bakePathText.length() < SCENE_BAKE_PATH_MAX) {
        copyBounded(scene->bakePath, sizeof(scene->bakePath), bakePathText.c_str());
    }
    if (modelJson.length() == 0) {
        modelJson = "{\"sceneSize\":1,\"wallHeight\":0.15,\"floors\":[{\"y\":0,\"height\":0.3,\"walls\":[],\"windows\":[],\"doors\":[]}]}";
    }
    if (!assignOwnedJson(&scene->modelJson, modelJson.c_str(), SCENE_MODEL_MAX)
        || !assignOwnedJson(&scene->pinsJson, pinsJson.c_str(), SCENE_PINS_JSON_MAX)) {
        freeScene(scene);
        return SceneWriteOutOfMemory;
    }
    *outScene = scene;
    return SceneWriteOk;
}

SceneWriteResult SceneStore::parseArray(const char *arrayJson, SceneRecord **outHead, int *outCount) const {
    if (arrayJson == nullptr || outHead == nullptr || outCount == nullptr) {
        return SceneWriteParseError;
    }
    *outHead = nullptr;
    *outCount = 0;
    SceneRecord *listHead = nullptr;
    SceneRecord *tail = nullptr;
    int count = 0;
    const char *cursor = arrayJson;
    while (*cursor != '\0' && isJsonSpace(*cursor)) {
        cursor++;
    }
    if (*cursor != '[') {
        return SceneWriteParseError;
    }
    cursor++;
    while (*cursor != '\0') {
        while (*cursor != '\0' && (isJsonSpace(*cursor) || *cursor == ',')) {
            cursor++;
        }
        if (*cursor == ']') {
            break;
        }
        if (*cursor != '{') {
            freeSceneList(listHead);
            return SceneWriteParseError;
        }
        const char *end = findJsonObjectEnd(cursor);
        if (end == nullptr) {
            freeSceneList(listHead);
            return SceneWriteParseError;
        }
        if (count >= SCENE_STORE_MAX) {
            freeSceneList(listHead);
            return SceneWriteFull;
        }
        String object;
        if (!assignJsonSlice(cursor, end, object)) {
            freeSceneList(listHead);
            return SceneWriteOutOfMemory;
        }
        SceneRecord *scene = nullptr;
        const SceneWriteResult parsed = parseOneObject(object.c_str(), &scene);
        if (parsed != SceneWriteOk || scene == nullptr) {
            freeSceneList(listHead);
            return parsed == SceneWriteOk ? SceneWriteParseError : parsed;
        }
        if (scene->id[0] == '\0' || !isIdSafe(scene->id)) {
            freeScene(scene);
            freeSceneList(listHead);
            return SceneWriteBadId;
        }
        for (SceneRecord *existing = listHead; existing != nullptr; existing = existing->next) {
            if (strcmp(existing->id, scene->id) == 0) {
                freeScene(scene);
                freeSceneList(listHead);
                return SceneWriteDuplicateId;
            }
        }
        scene->next = nullptr;
        if (tail == nullptr) {
            listHead = scene;
        } else {
            tail->next = scene;
        }
        tail = scene;
        count++;
        cursor = end + 1;
    }
    *outHead = listHead;
    *outCount = count;
    return SceneWriteOk;
}

bool SceneStore::saveToFile() const {
    PsramJsonBuffer json;
    json += "[";
    bool firstItem = true;
    for (const SceneRecord *scene = head; scene != nullptr; scene = scene->next) {
        if (!firstItem) {
            json += ",";
        }
        firstItem = false;
        appendSceneFull(json, scene);
    }
    json += "]";
    if (!json.ok()) {
        return false;
    }
    File file = LittleFS.open(SCENES_STORE_PATH, "w");
    if (!file) {
        LOGGER.error("Scenes file write failed");
        return false;
    }
    const bool written = json.writeToFile(file);
    file.close();
    return written;
}

bool SceneStore::loadFromFile() {
    File file = LittleFS.open(SCENES_STORE_PATH, "r");
    if (!file) {
        return false;
    }
    PsramJsonBuffer json;
    const bool loaded = json.assignFromFile(file);
    file.close();
    if (!loaded || !json.ok()) {
        return false;
    }
    json.trim();
    if (json.length() == 0 || json.charAt(0) != '[') {
        return false;
    }
    SceneRecord *newHead = nullptr;
    int count = 0;
    const SceneWriteResult result = parseArray(json.c_str(), &newHead, &count);
    if (result != SceneWriteOk) {
        LOGGER.error(String("Scenes file load failed: ") + writeResultText(result));
        return false;
    }
    clearAll();
    head = newHead;
    storedCount = count;
    return true;
}

void SceneStore::appendSceneFull(PsramJsonBuffer &json, const SceneRecord *scene) const {
    const bool hasBake = scene->bakePath[0] != '\0' && LittleFS.exists(scene->bakePath);
    json += "{\"id\":\"";
    appendJsonEscaped(json, scene->id, SCENE_ID_BUF);
    json += "\",\"name\":\"";
    appendJsonEscaped(json, scene->name, SCENE_NAME_MAX);
    json += "\",\"bakePath\":\"";
    appendJsonEscaped(json, scene->bakePath, SCENE_BAKE_PATH_MAX);
    json += "\",\"hasBake\":";
    json += hasBake ? "true" : "false";
    json += ",\"sceneModel\":";
    json += scene->modelJson != nullptr ? scene->modelJson : "{}";
    json += ",\"pins\":";
    json += scene->pinsJson != nullptr ? scene->pinsJson : "[]";
    json += "}";
}

void SceneStore::appendSceneListItem(PsramJsonBuffer &json, const SceneRecord *scene) const {
    const bool hasBake = scene->bakePath[0] != '\0' && LittleFS.exists(scene->bakePath);
    json += "{\"id\":\"";
    appendJsonEscaped(json, scene->id, SCENE_ID_BUF);
    json += "\",\"name\":\"";
    appendJsonEscaped(json, scene->name, SCENE_NAME_MAX);
    json += "\",\"bakePath\":\"";
    appendJsonEscaped(json, scene->bakePath, SCENE_BAKE_PATH_MAX);
    json += "\",\"hasBake\":";
    json += hasBake ? "true" : "false";
    json += "}";
}

String SceneStore::listJson() const {
    PsramJsonBuffer json;
    json += "[";
    bool firstItem = true;
    for (const SceneRecord *scene = head; scene != nullptr; scene = scene->next) {
        if (!firstItem) {
            json += ",";
        }
        firstItem = false;
        appendSceneListItem(json, scene);
    }
    json += "]";
    if (!json.ok()) {
        return String();
    }
    return String(json.c_str());
}

String SceneStore::sceneFullJson(const SceneRecord *scene) const {
    if (scene == nullptr) {
        return String();
    }
    PsramJsonBuffer json;
    appendSceneFull(json, scene);
    if (!json.ok()) {
        return String();
    }
    return String(json.c_str());
}

String SceneStore::exportJson() const {
    PsramJsonBuffer json;
    json += "[";
    bool firstItem = true;
    for (const SceneRecord *scene = head; scene != nullptr; scene = scene->next) {
        if (!firstItem) {
            json += ",";
        }
        firstItem = false;
        appendSceneFull(json, scene);
    }
    json += "]";
    if (!json.ok()) {
        return String();
    }
    return String(json.c_str());
}

SceneWriteResult SceneStore::createFromJson(const char *json, SceneRecord **createdOut) {
    if (createdOut != nullptr) {
        *createdOut = nullptr;
    }
    if (storedCount >= SCENE_STORE_MAX) {
        return SceneWriteFull;
    }
    SceneRecord *scene = nullptr;
    const SceneWriteResult parsed = parseOneObject(json, &scene);
    if (parsed != SceneWriteOk || scene == nullptr) {
        return parsed;
    }
    if (scene->id[0] == '\0') {
        generateUniqueId(scene->id);
    } else if (!isIdSafe(scene->id)) {
        freeScene(scene);
        return SceneWriteBadId;
    } else if (findById(scene->id) != nullptr) {
        freeScene(scene);
        return SceneWriteDuplicateId;
    }
    if (scene->bakePath[0] == '\0') {
        snprintf(scene->bakePath, sizeof(scene->bakePath), "%s/%s.jpg", SCENE_BAKES_DIR, scene->id);
    }
    scene->next = head;
    head = scene;
    storedCount++;
    requestPersist();
    if (createdOut != nullptr) {
        *createdOut = scene;
    }
    return SceneWriteOk;
}

SceneWriteResult SceneStore::replaceFromJson(const char *sceneId, const char *json, bool keepId) {
    SceneRecord *existing = findById(sceneId);
    if (existing == nullptr) {
        return SceneWriteNotFound;
    }
    SceneRecord *replacement = nullptr;
    const SceneWriteResult parsed = parseOneObject(json, &replacement);
    if (parsed != SceneWriteOk || replacement == nullptr) {
        return parsed;
    }
    if (keepId || replacement->id[0] == '\0') {
        copyBounded(replacement->id, sizeof(replacement->id), existing->id);
    } else if (!isIdSafe(replacement->id)) {
        freeScene(replacement);
        return SceneWriteBadId;
    } else if (strcmp(replacement->id, existing->id) != 0 && findById(replacement->id) != nullptr) {
        freeScene(replacement);
        return SceneWriteDuplicateId;
    }
    if (replacement->bakePath[0] == '\0') {
        if (existing->bakePath[0] != '\0') {
            copyBounded(replacement->bakePath, sizeof(replacement->bakePath), existing->bakePath);
        } else {
            snprintf(replacement->bakePath, sizeof(replacement->bakePath), "%s/%s.jpg", SCENE_BAKES_DIR, replacement->id);
        }
    }
    free(existing->modelJson);
    free(existing->pinsJson);
    copyBounded(existing->id, sizeof(existing->id), replacement->id);
    copyBounded(existing->name, sizeof(existing->name), replacement->name);
    copyBounded(existing->bakePath, sizeof(existing->bakePath), replacement->bakePath);
    existing->modelJson = replacement->modelJson;
    existing->pinsJson = replacement->pinsJson;
    replacement->modelJson = nullptr;
    replacement->pinsJson = nullptr;
    freeScene(replacement);
    requestPersist();
    return SceneWriteOk;
}

void SceneStore::deleteBakeFile(const SceneRecord *scene) const {
    if (scene == nullptr || scene->bakePath[0] == '\0') {
        return;
    }
    if (LittleFS.exists(scene->bakePath)) {
        LittleFS.remove(scene->bakePath);
    }
}

SceneWriteResult SceneStore::deleteById(const char *sceneId) {
    if (sceneId == nullptr || sceneId[0] == '\0') {
        return SceneWriteBadId;
    }
    SceneRecord *previous = nullptr;
    for (SceneRecord *scene = head; scene != nullptr; scene = scene->next) {
        if (strcmp(scene->id, sceneId) == 0) {
            if (previous == nullptr) {
                head = scene->next;
            } else {
                previous->next = scene->next;
            }
            deleteBakeFile(scene);
            freeScene(scene);
            storedCount--;
            requestPersist();
            return SceneWriteOk;
        }
        previous = scene;
    }
    return SceneWriteNotFound;
}

SceneWriteResult SceneStore::replaceAllFromJson(const String &arrayJson) {
    return replaceAllFromJson(arrayJson.c_str());
}

SceneWriteResult SceneStore::replaceAllFromJson(const char *arrayJson) {
    SceneRecord *newHead = nullptr;
    int count = 0;
    const SceneWriteResult parsed = parseArray(arrayJson, &newHead, &count);
    if (parsed != SceneWriteOk) {
        return parsed;
    }
    for (SceneRecord *oldScene = head; oldScene != nullptr; oldScene = oldScene->next) {
        bool keepBake = false;
        for (SceneRecord *newScene = newHead; newScene != nullptr; newScene = newScene->next) {
            if (strcmp(oldScene->id, newScene->id) == 0
                && strcmp(oldScene->bakePath, newScene->bakePath) == 0) {
                keepBake = true;
                break;
            }
        }
        if (!keepBake) {
            deleteBakeFile(oldScene);
        }
    }
    clearAll();
    head = newHead;
    storedCount = count;
    requestPersist();
    return SceneWriteOk;
}

SceneWriteResult SceneStore::importOnlyNewFromJson(
    const char *arrayJson,
    int *addedOut,
    int *skippedOut
) {
    if (addedOut != nullptr) {
        *addedOut = 0;
    }
    if (skippedOut != nullptr) {
        *skippedOut = 0;
    }
    SceneRecord *incoming = nullptr;
    int count = 0;
    const SceneWriteResult parsed = parseArray(arrayJson, &incoming, &count);
    if (parsed != SceneWriteOk) {
        return parsed;
    }
    int added = 0;
    int skipped = 0;
    SceneRecord *scene = incoming;
    while (scene != nullptr) {
        SceneRecord *next = scene->next;
        scene->next = nullptr;
        if (scene->id[0] != '\0' && findById(scene->id) != nullptr) {
            freeScene(scene);
            skipped++;
            scene = next;
            continue;
        }
        if (storedCount >= SCENE_STORE_MAX) {
            freeScene(scene);
            freeSceneList(next);
            if (addedOut != nullptr) {
                *addedOut = added;
            }
            if (skippedOut != nullptr) {
                *skippedOut = skipped;
            }
            if (added > 0) {
                requestPersist();
            }
            return SceneWriteFull;
        }
        if (scene->id[0] == '\0') {
            generateUniqueId(scene->id);
        } else if (!isIdSafe(scene->id)) {
            freeScene(scene);
            freeSceneList(next);
            if (addedOut != nullptr) {
                *addedOut = added;
            }
            if (skippedOut != nullptr) {
                *skippedOut = skipped;
            }
            if (added > 0) {
                requestPersist();
            }
            return SceneWriteBadId;
        }
        if (scene->bakePath[0] == '\0') {
            snprintf(scene->bakePath, sizeof(scene->bakePath), "%s/%s.jpg", SCENE_BAKES_DIR, scene->id);
        }
        scene->next = head;
        head = scene;
        storedCount++;
        added++;
        scene = next;
    }
    if (addedOut != nullptr) {
        *addedOut = added;
    }
    if (skippedOut != nullptr) {
        *skippedOut = skipped;
    }
    if (added > 0) {
        requestPersist();
    }
    return SceneWriteOk;
}

SceneWriteResult SceneStore::writeBakeJpeg(const char *sceneId, const uint8_t *data, size_t length) {
    SceneRecord *scene = findById(sceneId);
    if (scene == nullptr) {
        return SceneWriteNotFound;
    }
    if (data == nullptr || length == 0 || length > 512 * 1024) {
        return SceneWriteBakeFailed;
    }
    ensureBakesDir();
    if (scene->bakePath[0] == '\0') {
        snprintf(scene->bakePath, sizeof(scene->bakePath), "%s/%s.jpg", SCENE_BAKES_DIR, scene->id);
    }
    File file = LittleFS.open(scene->bakePath, "w");
    if (!file) {
        return SceneWriteBakeFailed;
    }
    const size_t written = file.write(data, length);
    file.close();
    if (written != length) {
        LittleFS.remove(scene->bakePath);
        return SceneWriteBakeFailed;
    }
    requestPersist();
    return SceneWriteOk;
}

bool SceneStore::readBakeJpeg(const char *sceneId, uint8_t **dataOut, size_t *lengthOut) const {
    if (dataOut == nullptr || lengthOut == nullptr) {
        return false;
    }
    *dataOut = nullptr;
    *lengthOut = 0;
    const SceneRecord *scene = findById(sceneId);
    if (scene == nullptr || scene->bakePath[0] == '\0' || !LittleFS.exists(scene->bakePath)) {
        return false;
    }
    File file = LittleFS.open(scene->bakePath, "r");
    if (!file) {
        return false;
    }
    const size_t length = file.size();
    if (length == 0 || length > 512 * 1024) {
        file.close();
        return false;
    }
    uint8_t *buffer = (uint8_t *)allocatePsram(length);
    if (buffer == nullptr) {
        file.close();
        return false;
    }
    const size_t readCount = file.read(buffer, length);
    file.close();
    if (readCount != length) {
        free(buffer);
        return false;
    }
    *dataOut = buffer;
    *lengthOut = length;
    return true;
}
