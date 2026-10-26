#include "UserStore.h"
#include "JsonField.h"
#include "Logger.h"

#include <LittleFS.h>
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>
#include <esp_random.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>

static_assert(USER_NAME_MAX == SPI_USER_SYNC_NAME_LEN, "user name SPI width must match store");
static_assert(USER_PASSWORD_SALT_LEN == SPI_USER_SYNC_SALT_LEN, "user salt SPI width must match store");
static_assert(USER_PASSWORD_HASH_LEN == SPI_USER_SYNC_HASH_LEN, "user hash SPI width must match store");

UserStore::UserStore()
    : usersHead(nullptr), storedCount(0), persistEnabled(false), persistPending(false), changedHandler(nullptr) {
    clearUsers();
    memset(sessions, 0, sizeof(sessions));
}

UserStore::~UserStore() {
    clearUsers();
}

bool UserStore::heapAllowsAllocation(size_t bytes) {
    if (bytes == 0) {
        return true;
    }
    const size_t freeHeap = ESP.getFreeHeap();
    const size_t maxBlock = ESP.getMaxAllocHeap();
    if (maxBlock < bytes || freeHeap < bytes) {
        return false;
    }
    if (freeHeap - bytes < CONSOLE_HEAP_RESERVE_BYTES) {
        return false;
    }
    return true;
}

void UserStore::freeConsoleIdList(UserConsoleIdNode *&listHead) {
    while (listHead != nullptr) {
        UserConsoleIdNode *next = listHead->next;
        free(listHead);
        listHead = next;
    }
    listHead = nullptr;
}

void UserStore::clearUserRecord(UserRecord *user) {
    if (user == nullptr) {
        return;
    }
    UserConsoleIdNode *consoleIds = user->consoleIds;
    user->consoleIds = nullptr;
    freeConsoleIdList(consoleIds);
    memset(user, 0, sizeof(*user));
}

bool UserStore::cloneConsoleIdList(UserConsoleIdNode **destination, const UserConsoleIdNode *source) {
    if (destination == nullptr) {
        return false;
    }
    *destination = nullptr;
    UserConsoleIdNode *tail = nullptr;
    for (const UserConsoleIdNode *node = source; node != nullptr; node = node->next) {
        if (!heapAllowsAllocation(sizeof(UserConsoleIdNode))) {
            freeConsoleIdList(*destination);
            return false;
        }
        UserConsoleIdNode *copy = (UserConsoleIdNode *)malloc(sizeof(UserConsoleIdNode));
        if (copy == nullptr) {
            freeConsoleIdList(*destination);
            return false;
        }
        memset(copy, 0, sizeof(*copy));
        strncpy(copy->id, node->id, sizeof(copy->id) - 1);
        copy->next = nullptr;
        if (tail == nullptr) {
            *destination = copy;
        } else {
            tail->next = copy;
        }
        tail = copy;
    }
    return true;
}

bool UserStore::parseConsoleIdListFromObject(const char *objectJson, UserConsoleIdNode **outHead) {
    if (outHead == nullptr) {
        return false;
    }
    *outHead = nullptr;
    String consolesJson;
    if (!extractJsonKeyedSlice(objectJson, "consoles", '[', consolesJson)) {
        return true;
    }
    UserConsoleIdNode *tail = nullptr;
    int count = 0;
    const char *cursor = consolesJson.c_str();
    while (cursor != nullptr && *cursor != '\0' && count < CONSOLE_STORE_MAX) {
        const char *quote = strchr(cursor, '"');
        if (quote == nullptr) {
            break;
        }
        quote++;
        const char *end = strchr(quote, '"');
        if (end == nullptr) {
            freeConsoleIdList(*outHead);
            return false;
        }
        String idText = String(quote).substring(0, (unsigned int)(end - quote));
        if (idText.length() > 0 && idText.length() < CONSOLE_ID_BUF) {
            if (!heapAllowsAllocation(sizeof(UserConsoleIdNode))) {
                freeConsoleIdList(*outHead);
                return false;
            }
            UserConsoleIdNode *node = (UserConsoleIdNode *)malloc(sizeof(UserConsoleIdNode));
            if (node == nullptr) {
                freeConsoleIdList(*outHead);
                return false;
            }
            memset(node, 0, sizeof(*node));
            strncpy(node->id, idText.c_str(), sizeof(node->id) - 1);
            node->next = nullptr;
            if (tail == nullptr) {
                *outHead = node;
            } else {
                tail->next = node;
            }
            tail = node;
            count++;
        }
        cursor = end + 1;
    }
    return true;
}

bool UserStore::userHasConsole(const UserRecord *user, const char *consoleId) {
    if (user == nullptr || consoleId == nullptr || consoleId[0] == '\0') {
        return false;
    }
    for (const UserConsoleIdNode *node = user->consoleIds; node != nullptr; node = node->next) {
        if (strcmp(node->id, consoleId) == 0) {
            return true;
        }
    }
    return false;
}

int UserStore::consoleIdCount(const UserRecord *user) {
    int count = 0;
    if (user == nullptr) {
        return 0;
    }
    for (const UserConsoleIdNode *node = user->consoleIds; node != nullptr; node = node->next) {
        count++;
    }
    return count;
}

void UserStore::appendConsolesJson(String &json, const UserRecord *user) {
    json += ",\"consoles\":[";
    bool first = true;
    if (user != nullptr) {
        for (const UserConsoleIdNode *node = user->consoleIds; node != nullptr; node = node->next) {
            if (!first) {
                json += ",";
            }
            first = false;
            json += "\"";
            appendJsonEscaped(json, node->id, sizeof(node->id));
            json += "\"";
        }
    }
    json += "]";
}

void UserStore::freeUserRecord(UserRecord *user) {
    if (user == nullptr) {
        return;
    }
    clearUserRecord(user);
    free(user);
}

UserRecord *UserStore::allocateUserRecord() {
    if (storedCount >= USER_STORE_MAX) {
        return nullptr;
    }
    UserRecord *user = (UserRecord *)calloc(1, sizeof(UserRecord));
    if (user == nullptr) {
        return nullptr;
    }
    user->next = usersHead;
    usersHead = user;
    storedCount++;
    return user;
}

void UserStore::clearUsers() {
    while (usersHead != nullptr) {
        UserRecord *next = usersHead->next;
        freeUserRecord(usersHead);
        usersHead = next;
    }
    storedCount = 0;
}

int UserStore::userCount() const {
    return storedCount;
}

int UserStore::nextUsedIndex(int startIndex) const {
    if (startIndex < 0) {
        startIndex = 0;
    }
    int ordinal = 0;
    for (const UserRecord *user = usersHead; user != nullptr; user = user->next) {
        if (user->userName[0] == '\0') {
            continue;
        }
        if (ordinal >= startIndex) {
            return ordinal;
        }
        ordinal++;
    }
    return -1;
}

UserRecord *UserStore::userAt(int index) {
    return const_cast<UserRecord *>(static_cast<const UserStore *>(this)->userAt(index));
}

const UserRecord *UserStore::userAt(int index) const {
    if (index < 0) {
        return nullptr;
    }
    int ordinal = 0;
    for (const UserRecord *user = usersHead; user != nullptr; user = user->next) {
        if (user->userName[0] == '\0') {
            continue;
        }
        if (ordinal == index) {
            return user;
        }
        ordinal++;
    }
    return nullptr;
}

void UserStore::setChangedHandler(ChangedFn handler) {
    changedHandler = handler;
}

bool UserStore::nameEquals(const char *left, const char *right) {
    if (left == nullptr || right == nullptr) {
        return false;
    }
    return strcmp(left, right) == 0;
}

const char *UserStore::themeJsonId(uint8_t themeId) {
    return themeId == UI_THEME_DARK ? "dark" : "light";
}

uint8_t UserStore::themeFromJsonId(const char *themeId) {
    if (themeId != nullptr && strcmp(themeId, "dark") == 0) {
        return UI_THEME_DARK;
    }
    return UI_THEME_LIGHT;
}

void UserStore::bytesToHex(const uint8_t *bytes, size_t length, char *hex) {
    static const char digits[] = "0123456789abcdef";
    for (size_t index = 0; index < length; index++) {
        hex[index * 2] = digits[bytes[index] >> 4];
        hex[index * 2 + 1] = digits[bytes[index] & 0x0F];
    }
    hex[length * 2] = '\0';
}

bool UserStore::hexToBytes(const char *hex, uint8_t *bytes, size_t length) {
    if (hex == nullptr || bytes == nullptr) {
        return false;
    }
    for (size_t index = 0; index < length; index++) {
        unsigned int value = 0;
        if (sscanf(hex + index * 2, "%2x", &value) != 1) {
            return false;
        }
        bytes[index] = (uint8_t)value;
    }
    return true;
}

void UserStore::hashPassword(const uint8_t *salt, const char *password, uint8_t *hashOut) {
    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    mbedtls_sha256_starts(&context, 0);
    mbedtls_sha256_update(&context, salt, USER_PASSWORD_SALT_LEN);
    if (password != nullptr) {
        mbedtls_sha256_update(&context, (const unsigned char *)password, strlen(password));
    }
    mbedtls_sha256_finish(&context, hashOut);
    mbedtls_sha256_free(&context);
}

bool UserStore::hashesEqual(const uint8_t *left, const uint8_t *right, size_t length) {
    uint8_t mix = 0;
    for (size_t index = 0; index < length; index++) {
        mix = (uint8_t)(mix | (left[index] ^ right[index]));
    }
    return mix == 0;
}

bool UserStore::actorMayEditUsers(const UserRecord *actor) {
    return actor != nullptr && (actor->isAdmin || actor->editUsers);
}

void UserStore::copyRolesAndFlags(UserRecord *destination, const UserRecord *source, bool replaceConsoles) {
    destination->isAdmin = source->isAdmin;
    destination->editDevices = source->editDevices;
    destination->addDevices = source->addDevices;
    destination->removeDevices = source->removeDevices;
    destination->editUsers = source->editUsers;
    destination->editConsoles = source->editConsoles;
    destination->monitor = source->monitor;
    destination->controlDevices = source->controlDevices;
    destination->isBlocked = source->isBlocked;
    destination->theme = source->theme == UI_THEME_DARK ? UI_THEME_DARK : UI_THEME_LIGHT;
    if (replaceConsoles) {
        freeConsoleIdList(destination->consoleIds);
        cloneConsoleIdList(&destination->consoleIds, source->consoleIds);
    }
}

UserRecord *UserStore::findByName(const char *userName) {
    return const_cast<UserRecord *>(static_cast<const UserStore *>(this)->findByName(userName));
}

const UserRecord *UserStore::findByName(const char *userName) const {
    if (userName == nullptr || userName[0] == '\0') {
        return nullptr;
    }
    for (const UserRecord *user = usersHead; user != nullptr; user = user->next) {
        if (nameEquals(user->userName, userName)) {
            return user;
        }
    }
    return nullptr;
}

int UserStore::adminCount() const {
    int count = 0;
    for (const UserRecord *user = usersHead; user != nullptr; user = user->next) {
        if (user->isAdmin) {
            count++;
        }
    }
    return count;
}

int UserStore::unlockedAdminCount() const {
    int count = 0;
    for (const UserRecord *user = usersHead; user != nullptr; user = user->next) {
        if (user->isAdmin && !user->isBlocked) {
            count++;
        }
    }
    return count;
}

bool UserStore::setPassword(UserRecord *user, const char *password) {
    if (user == nullptr || password == nullptr || password[0] == '\0') {
        return false;
    }
    uint8_t salt[USER_PASSWORD_SALT_LEN];
    uint8_t hash[USER_PASSWORD_HASH_LEN];
    esp_fill_random(salt, sizeof(salt));
    hashPassword(salt, password, hash);
    bytesToHex(salt, sizeof(salt), user->passwordSaltHex);
    bytesToHex(hash, sizeof(hash), user->passwordHashHex);
    clearSessionsForUser(user->userName);
    return true;
}

bool UserStore::passwordMatches(const UserRecord *user, const char *password) const {
    if (user == nullptr || password == nullptr) {
        return false;
    }
    uint8_t salt[USER_PASSWORD_SALT_LEN];
    uint8_t storedHash[USER_PASSWORD_HASH_LEN];
    uint8_t computedHash[USER_PASSWORD_HASH_LEN];
    if (!hexToBytes(user->passwordSaltHex, salt, sizeof(salt))) {
        return false;
    }
    if (!hexToBytes(user->passwordHashHex, storedHash, sizeof(storedHash))) {
        return false;
    }
    hashPassword(salt, password, computedHash);
    return hashesEqual(storedHash, computedHash, sizeof(storedHash));
}

void UserStore::seedAdmin() {
    clearUsers();
    UserRecord *admin = allocateUserRecord();
    if (admin == nullptr) {
        return;
    }
    strncpy(admin->userName, "admin", sizeof(admin->userName) - 1);
    admin->isAdmin = true;
    admin->theme = UI_THEME_DARK;
    admin->isBlocked = false;
    time_t wallClock = time(nullptr);
    admin->addedAt = wallClock > 1600000000 ? (uint32_t)wallClock : 0;
    setPassword(admin, "admin");
    LOGGER.info("Seeded console user admin");
}

bool UserStore::saveToFile() const {
    File file = LittleFS.open(USERS_STORE_PATH, "w");
    if (!file) {
        LOGGER.error("Users file write failed");
        return false;
    }
    const String json = listExportJson();
    const size_t written = file.print(json);
    file.close();
    return written == json.length();
}

bool UserStore::parseUsersJson(const String &json, bool requireHashes) {
    clearUsers();
    const char *cursor = json.c_str();
    while (cursor != nullptr && *cursor != '\0' && storedCount < USER_STORE_MAX) {
        const char *objectStart = strchr(cursor, '{');
        if (objectStart == nullptr) {
            break;
        }
        const char *objectEnd = strchr(objectStart, '}');
        if (objectEnd == nullptr) {
            break;
        }
        String object = String(objectStart).substring(0, (unsigned int)(objectEnd - objectStart + 1));
        String userName;
        String saltHex;
        String hashHex;
        String themeText;
        extractJsonString(object.c_str(), "userName", userName);
        extractJsonString(object.c_str(), "passwordSalt", saltHex);
        extractJsonString(object.c_str(), "passwordHash", hashHex);
        extractJsonString(object.c_str(), "theme", themeText);
        if (userName.length() == 0 || userName.length() >= USER_NAME_MAX) {
            cursor = objectEnd + 1;
            continue;
        }
        if (findByName(userName.c_str()) != nullptr) {
            cursor = objectEnd + 1;
            continue;
        }
        if (requireHashes && (saltHex.length() != USER_PASSWORD_SALT_LEN * 2 || hashHex.length() != USER_PASSWORD_HASH_LEN * 2)) {
            cursor = objectEnd + 1;
            continue;
        }
        UserRecord *user = allocateUserRecord();
        if (user == nullptr) {
            cursor = objectEnd + 1;
            continue;
        }
        strncpy(user->userName, userName.c_str(), sizeof(user->userName) - 1);
        strncpy(user->passwordSaltHex, saltHex.c_str(), sizeof(user->passwordSaltHex) - 1);
        strncpy(user->passwordHashHex, hashHex.c_str(), sizeof(user->passwordHashHex) - 1);
        int addedAt = 0;
        extractJsonInt(object.c_str(), "addedAt", addedAt);
        user->addedAt = addedAt > 0 ? (uint32_t)addedAt : 0;
        extractJsonBool(object.c_str(), "isAdmin", user->isAdmin);
        extractJsonBool(object.c_str(), "editDevices", user->editDevices);
        extractJsonBool(object.c_str(), "addDevices", user->addDevices);
        extractJsonBool(object.c_str(), "removeDevices", user->removeDevices);
        extractJsonBool(object.c_str(), "editUsers", user->editUsers);
        extractJsonBool(object.c_str(), "editConsoles", user->editConsoles);
        extractJsonBool(object.c_str(), "monitor", user->monitor);
        extractJsonBool(object.c_str(), "controlDevices", user->controlDevices);
        extractJsonBool(object.c_str(), "isBlocked", user->isBlocked);
        user->theme = themeFromJsonId(themeText.c_str());
        if (!parseConsoleIdListFromObject(object.c_str(), &user->consoleIds)) {
            clearUserRecord(user);
            cursor = objectEnd + 1;
            continue;
        }
        cursor = objectEnd + 1;
    }
    return storedCount > 0;
}

bool UserStore::loadFromFile() {
    File file = LittleFS.open(USERS_STORE_PATH, "r");
    if (!file) {
        return false;
    }
    String json = file.readString();
    file.close();
    json.trim();
    if (json.length() == 0 || json.charAt(0) != '[') {
        return false;
    }
    return parseUsersJson(json, true);
}

bool UserStore::begin(bool persistToLittleFs) {
    persistEnabled = persistToLittleFs;
    if (!persistEnabled) {
        return true;
    }
    if (loadFromFile()) {
        LOGGER.info("Loaded " + String(storedCount) + " console user(s)");
        return true;
    }
    seedAdmin();
    if (!persistNow()) {
        LOGGER.error("User seed persist failed");
        return false;
    }
    return storedCount > 0;
}

bool UserStore::persistNow() {
    if (!persistEnabled) {
        return true;
    }
    if (storedCount == 0) {
        LOGGER.warning("Refusing to erase the user store with an empty list");
        loadFromFile();
        return false;
    }
    if (!saveToFile()) {
        LOGGER.error("User store write failed");
        return false;
    }
    LOGGER.info("User store saved " + String(storedCount) + " user(s)");
    return true;
}

void UserStore::requestPersist() {
    persistPending = true;
}

void UserStore::persistIfDue() {
    if (!persistPending) {
        return;
    }
    if (!persistEnabled) {
        persistPending = false;
        return;
    }
    persistPending = false;
    if (storedCount == 0) {
        LOGGER.warning("Refusing to erase the user store with an empty list");
        loadFromFile();
        return;
    }
    if (!saveToFile()) {
        LOGGER.error("User store write failed");
        persistPending = true;
        return;
    }
    LOGGER.info("User store saved " + String(storedCount) + " user(s)");
}

void UserStore::afterMutation(UserChangeKind kind, const UserRecord *user) {
    if (persistEnabled) {
        requestPersist();
    }
    if (changedHandler != nullptr && user != nullptr) {
        changedHandler(kind, user);
    }
}

void UserStore::noteRecordChanged(const UserRecord *user) {
    afterMutation(UserChangeUpsert, user);
}

void UserStore::replaceFrom(const UserStore *source) {
    if (source == nullptr) {
        return;
    }
    clearUsers();
    for (const UserRecord *user = source->usersHead; user != nullptr; user = user->next) {
        upsertFromRecord(user, false);
    }
}

void UserStore::resetToEmpty() {
    clearUsers();
    memset(sessions, 0, sizeof(sessions));
    persistPending = false;
}

bool UserStore::upsertFromRecord(const UserRecord *source, bool notify) {
    if (source == nullptr || source->userName[0] == '\0') {
        return false;
    }
    UserRecord *existing = findByName(source->userName);
    if (existing != nullptr) {
        UserConsoleIdNode *keptConsoles = existing->consoleIds;
        existing->consoleIds = nullptr;
        strncpy(existing->userName, source->userName, sizeof(existing->userName) - 1);
        strncpy(existing->passwordSaltHex, source->passwordSaltHex, sizeof(existing->passwordSaltHex) - 1);
        strncpy(existing->passwordHashHex, source->passwordHashHex, sizeof(existing->passwordHashHex) - 1);
        existing->addedAt = source->addedAt;
        copyRolesAndFlags(existing, source, source->consoleIds != nullptr);
        if (source->consoleIds == nullptr) {
            existing->consoleIds = keptConsoles;
        } else {
            freeConsoleIdList(keptConsoles);
        }
        if (notify) {
            afterMutation(UserChangeUpsert, existing);
        }
        return true;
    }
    UserRecord *user = allocateUserRecord();
    if (user == nullptr) {
        return false;
    }
    strncpy(user->userName, source->userName, sizeof(user->userName) - 1);
    strncpy(user->passwordSaltHex, source->passwordSaltHex, sizeof(user->passwordSaltHex) - 1);
    strncpy(user->passwordHashHex, source->passwordHashHex, sizeof(user->passwordHashHex) - 1);
    user->addedAt = source->addedAt;
    copyRolesAndFlags(user, source, true);
    if (notify) {
        afterMutation(UserChangeUpsert, user);
    }
    return true;
}

bool UserStore::removeByName(const char *userName, bool notify) {
    UserRecord **link = &usersHead;
    while (*link != nullptr) {
        UserRecord *user = *link;
        if (!nameEquals(user->userName, userName)) {
            link = &user->next;
            continue;
        }
        UserRecord removed = *user;
        removed.consoleIds = nullptr;
        removed.next = nullptr;
        *link = user->next;
        freeUserRecord(user);
        storedCount--;
        if (notify) {
            afterMutation(UserChangeDelete, &removed);
        }
        return true;
    }
    return false;
}

bool UserStore::replaceFromExportJson(
    const String &json,
    char (*removedNames)[USER_NAME_MAX],
    int *removedCount,
    int removedMax
) {
    UserStore scratch;
    if (!scratch.parseUsersJson(json, true) || scratch.unlockedAdminCount() < 1) {
        return false;
    }
    int collected = 0;
    for (const UserRecord *user = usersHead; user != nullptr; user = user->next) {
        if (scratch.findByName(user->userName) != nullptr) {
            continue;
        }
        if (removedNames != nullptr && collected < removedMax) {
            strncpy(removedNames[collected], user->userName, USER_NAME_MAX - 1);
            removedNames[collected][USER_NAME_MAX - 1] = '\0';
            collected++;
        }
    }
    if (removedCount != nullptr) {
        *removedCount = collected;
    }
    clearUsers();
    for (const UserRecord *user = scratch.usersHead; user != nullptr; user = user->next) {
        upsertFromRecord(user, false);
    }
    if (persistEnabled) {
        requestPersist();
    }
    return true;
}

size_t UserStore::packSyncPayload(uint8_t *out, size_t outMax, uint8_t flags, const UserRecord *user) {
    if (out == nullptr) {
        return 0;
    }
    if ((flags & SPI_USER_SYNC_RESET) != 0 || (flags & SPI_USER_SYNC_LAST) != 0) {
        if (outMax < 2 && (flags & SPI_USER_SYNC_RESET) != 0) {
            return 0;
        }
        if (outMax < 1) {
            return 0;
        }
        out[0] = flags;
        if ((flags & SPI_USER_SYNC_RESET) != 0) {
            return 1;
        }
        return 1;
    }
    if (user == nullptr || outMax < SPI_USER_SYNC_ENTRY_LEN) {
        return 0;
    }
    memset(out, 0, SPI_USER_SYNC_ENTRY_LEN);
    out[0] = flags;
    strncpy((char *)out + 1, user->userName, SPI_USER_SYNC_NAME_LEN - 1);
    if ((flags & SPI_USER_SYNC_DELETE) != 0) {
        return SPI_USER_SYNC_ENTRY_LEN;
    }
    uint8_t salt[SPI_USER_SYNC_SALT_LEN];
    uint8_t hash[SPI_USER_SYNC_HASH_LEN];
    if (!hexToBytes(user->passwordSaltHex, salt, sizeof(salt))
        || !hexToBytes(user->passwordHashHex, hash, sizeof(hash))) {
        return 0;
    }
    memcpy(out + 1 + SPI_USER_SYNC_NAME_LEN, salt, sizeof(salt));
    memcpy(out + 1 + SPI_USER_SYNC_NAME_LEN + SPI_USER_SYNC_SALT_LEN, hash, sizeof(hash));
    const size_t addedAtOffset = 1 + SPI_USER_SYNC_NAME_LEN + SPI_USER_SYNC_SALT_LEN + SPI_USER_SYNC_HASH_LEN;
    out[addedAtOffset] = (uint8_t)(user->addedAt & 0xFF);
    out[addedAtOffset + 1] = (uint8_t)((user->addedAt >> 8) & 0xFF);
    out[addedAtOffset + 2] = (uint8_t)((user->addedAt >> 16) & 0xFF);
    out[addedAtOffset + 3] = (uint8_t)((user->addedAt >> 24) & 0xFF);
    uint8_t roleFlags = 0;
    if (user->isAdmin) {
        roleFlags |= SPI_USER_FLAG_ADMIN;
    }
    if (user->editDevices) {
        roleFlags |= SPI_USER_FLAG_EDIT_DEVICES;
    }
    if (user->addDevices) {
        roleFlags |= SPI_USER_FLAG_ADD_DEVICES;
    }
    if (user->removeDevices) {
        roleFlags |= SPI_USER_FLAG_REMOVE_DEVICES;
    }
    if (user->editUsers) {
        roleFlags |= SPI_USER_FLAG_EDIT_USERS;
    }
    if (user->editConsoles) {
        roleFlags |= SPI_USER_FLAG_EDIT_CONSOLES;
    }
    if (user->monitor) {
        roleFlags |= SPI_USER_FLAG_MONITOR;
    }
    if (user->isBlocked) {
        roleFlags |= SPI_USER_FLAG_BLOCKED;
    }
    out[addedAtOffset + 4] = roleFlags;
    out[addedAtOffset + 5] = user->theme == UI_THEME_DARK ? UI_THEME_DARK : UI_THEME_LIGHT;
    uint8_t roleFlags2 = 0;
    if (user->controlDevices) {
        roleFlags2 |= SPI_USER_FLAG2_CONTROL_DEVICES;
    }
    out[addedAtOffset + 6] = roleFlags2;
    return SPI_USER_SYNC_ENTRY_LEN;
}

bool UserStore::unpackSyncPayload(const uint8_t *in, uint16_t length, uint8_t *flags, UserRecord *user) {
    if (in == nullptr || flags == nullptr || length < 1) {
        return false;
    }
    *flags = in[0];
    if ((*flags & SPI_USER_SYNC_RESET) != 0 || (*flags & SPI_USER_SYNC_LAST) != 0) {
        return true;
    }
    if (user == nullptr || length < 1 + SPI_USER_SYNC_NAME_LEN) {
        return false;
    }
    memset(user, 0, sizeof(*user));
    memcpy(user->userName, in + 1, SPI_USER_SYNC_NAME_LEN - 1);
    user->userName[USER_NAME_MAX - 1] = '\0';
    if ((*flags & SPI_USER_SYNC_DELETE) != 0) {
        return true;
    }
    if (length < SPI_USER_SYNC_ENTRY_LEN_LEGACY) {
        return false;
    }
    const uint8_t *salt = in + 1 + SPI_USER_SYNC_NAME_LEN;
    const uint8_t *hash = salt + SPI_USER_SYNC_SALT_LEN;
    bytesToHex(salt, SPI_USER_SYNC_SALT_LEN, user->passwordSaltHex);
    bytesToHex(hash, SPI_USER_SYNC_HASH_LEN, user->passwordHashHex);
    const size_t addedAtOffset = 1 + SPI_USER_SYNC_NAME_LEN + SPI_USER_SYNC_SALT_LEN + SPI_USER_SYNC_HASH_LEN;
    user->addedAt = (uint32_t)in[addedAtOffset] | ((uint32_t)in[addedAtOffset + 1] << 8)
        | ((uint32_t)in[addedAtOffset + 2] << 16) | ((uint32_t)in[addedAtOffset + 3] << 24);
    const uint8_t roleFlags = in[addedAtOffset + 4];
    user->isAdmin = (roleFlags & SPI_USER_FLAG_ADMIN) != 0;
    user->editDevices = (roleFlags & SPI_USER_FLAG_EDIT_DEVICES) != 0;
    user->addDevices = (roleFlags & SPI_USER_FLAG_ADD_DEVICES) != 0;
    user->removeDevices = (roleFlags & SPI_USER_FLAG_REMOVE_DEVICES) != 0;
    user->editUsers = (roleFlags & SPI_USER_FLAG_EDIT_USERS) != 0;
    user->editConsoles = (roleFlags & SPI_USER_FLAG_EDIT_CONSOLES) != 0;
    user->monitor = (roleFlags & SPI_USER_FLAG_MONITOR) != 0;
    user->isBlocked = (roleFlags & SPI_USER_FLAG_BLOCKED) != 0;
    user->theme = in[addedAtOffset + 5] == UI_THEME_DARK ? UI_THEME_DARK : UI_THEME_LIGHT;
    user->controlDevices = false;
    if (length >= SPI_USER_SYNC_ENTRY_LEN) {
        user->controlDevices = (in[addedAtOffset + 6] & SPI_USER_FLAG2_CONTROL_DEVICES) != 0;
    }
    user->consoleIds = nullptr;
    return true;
}

UserWriteResult UserStore::createUser(const UserRecord *actor, const UserRecord *source, const char *password) {
    if (!actorMayEditUsers(actor) || source == nullptr) {
        return UserWriteForbidden;
    }
    if (source->userName[0] == '\0') {
        return UserWriteBadName;
    }
    if (source->isAdmin && (actor == nullptr || !actor->isAdmin)) {
        return UserWriteForbidden;
    }
    if (findByName(source->userName) != nullptr) {
        return UserWriteDuplicate;
    }
    if (storedCount >= USER_STORE_MAX) {
        return UserWriteFull;
    }
    if (password == nullptr || password[0] == '\0') {
        return UserWriteNeedPassword;
    }
    UserRecord *user = allocateUserRecord();
    if (user == nullptr) {
        return UserWriteFull;
    }
    strncpy(user->userName, source->userName, sizeof(user->userName) - 1);
    copyRolesAndFlags(user, source, true);
    if (!actor->isAdmin) {
        user->isAdmin = false;
    }
    time_t wallClock = time(nullptr);
    user->addedAt = wallClock > 1600000000 ? (uint32_t)wallClock : 0;
    if (!setPassword(user, password)) {
        UserRecord **link = &usersHead;
        while (*link != user && *link != nullptr) {
            link = &(*link)->next;
        }
        if (*link == user) {
            *link = user->next;
            storedCount--;
        }
        freeUserRecord(user);
        return UserWriteNeedPassword;
    }
    if (unlockedAdminCount() < 1) {
        UserRecord **link = &usersHead;
        while (*link != user && *link != nullptr) {
            link = &(*link)->next;
        }
        if (*link == user) {
            *link = user->next;
            storedCount--;
        }
        freeUserRecord(user);
        return UserWriteLastAdmin;
    }
    afterMutation(UserChangeUpsert, user);
    return UserWriteOk;
}

UserWriteResult UserStore::updateUser(
    const UserRecord *actor,
    const char *userName,
    const UserRecord *source,
    const char *password
) {
    if (!actorMayEditUsers(actor) || source == nullptr) {
        return UserWriteForbidden;
    }
    UserRecord *user = findByName(userName);
    if (user == nullptr) {
        return UserWriteNotFound;
    }
    if (user->isAdmin && !actor->isAdmin) {
        return UserWriteForbidden;
    }
    if (source->isAdmin && !actor->isAdmin) {
        return UserWriteForbidden;
    }
    UserConsoleIdNode *previousConsoles = nullptr;
    if (!cloneConsoleIdList(&previousConsoles, user->consoleIds)) {
        return UserWriteOutOfMemory;
    }
    const bool previousAdmin = user->isAdmin;
    const bool previousEditDevices = user->editDevices;
    const bool previousAddDevices = user->addDevices;
    const bool previousRemoveDevices = user->removeDevices;
    const bool previousEditUsers = user->editUsers;
    const bool previousEditConsoles = user->editConsoles;
    const bool previousMonitor = user->monitor;
    const bool previousControlDevices = user->controlDevices;
    const bool previousBlocked = user->isBlocked;
    const uint8_t previousTheme = user->theme;
    char previousSalt[sizeof(user->passwordSaltHex)];
    char previousHash[sizeof(user->passwordHashHex)];
    memcpy(previousSalt, user->passwordSaltHex, sizeof(previousSalt));
    memcpy(previousHash, user->passwordHashHex, sizeof(previousHash));
    copyRolesAndFlags(user, source, true);
    if (!actor->isAdmin) {
        user->isAdmin = false;
    }
    if (unlockedAdminCount() < 1) {
        freeConsoleIdList(user->consoleIds);
        user->consoleIds = previousConsoles;
        user->isAdmin = previousAdmin;
        user->editDevices = previousEditDevices;
        user->addDevices = previousAddDevices;
        user->removeDevices = previousRemoveDevices;
        user->editUsers = previousEditUsers;
        user->editConsoles = previousEditConsoles;
        user->monitor = previousMonitor;
        user->controlDevices = previousControlDevices;
        user->isBlocked = previousBlocked;
        user->theme = previousTheme;
        return UserWriteLastAdmin;
    }
    if (password != nullptr && password[0] != '\0') {
        if (!setPassword(user, password)) {
            freeConsoleIdList(user->consoleIds);
            user->consoleIds = previousConsoles;
            user->isAdmin = previousAdmin;
            user->editDevices = previousEditDevices;
            user->addDevices = previousAddDevices;
            user->removeDevices = previousRemoveDevices;
            user->editUsers = previousEditUsers;
            user->editConsoles = previousEditConsoles;
            user->monitor = previousMonitor;
            user->controlDevices = previousControlDevices;
            user->isBlocked = previousBlocked;
            user->theme = previousTheme;
            memcpy(user->passwordSaltHex, previousSalt, sizeof(previousSalt));
            memcpy(user->passwordHashHex, previousHash, sizeof(previousHash));
            return UserWriteNeedPassword;
        }
    }
    freeConsoleIdList(previousConsoles);
    if (user->isBlocked || (password != nullptr && password[0] != '\0')) {
        clearSessionsForUser(user->userName);
    }
    afterMutation(UserChangeUpsert, user);
    return UserWriteOk;
}

UserWriteResult UserStore::deleteUser(const UserRecord *actor, const char *userName) {
    if (!actorMayEditUsers(actor)) {
        return UserWriteForbidden;
    }
    UserRecord *target = findByName(userName);
    if (target == nullptr) {
        return UserWriteNotFound;
    }
    if (target->isAdmin && !actor->isAdmin) {
        return UserWriteForbidden;
    }
    int remainingUnlockedAdmins = unlockedAdminCount();
    if (target->isAdmin && !target->isBlocked) {
        remainingUnlockedAdmins--;
    }
    if (remainingUnlockedAdmins < 1) {
        return UserWriteLastAdmin;
    }
    clearSessionsForUser(userName);
    if (!removeByName(userName, true)) {
        return UserWriteNotFound;
    }
    return UserWriteOk;
}

void UserStore::appendUserPublicJson(String &json, const UserRecord *user) const {
    json += "{\"userName\":\"";
    appendJsonEscaped(json, user->userName, sizeof(user->userName));
    json += "\",\"addedAt\":";
    json += String((unsigned long)user->addedAt);
    json += ",\"isAdmin\":";
    json += user->isAdmin ? "true" : "false";
    json += ",\"editDevices\":";
    json += user->editDevices ? "true" : "false";
    json += ",\"addDevices\":";
    json += user->addDevices ? "true" : "false";
    json += ",\"removeDevices\":";
    json += user->removeDevices ? "true" : "false";
    json += ",\"editUsers\":";
    json += user->editUsers ? "true" : "false";
    json += ",\"editConsoles\":";
    json += user->editConsoles ? "true" : "false";
    json += ",\"monitor\":";
    json += user->monitor ? "true" : "false";
    json += ",\"controlDevices\":";
    json += user->controlDevices ? "true" : "false";
    json += ",\"isBlocked\":";
    json += user->isBlocked ? "true" : "false";
    json += ",\"theme\":\"";
    json += themeJsonId(user->theme);
    json += "\"";
    appendConsolesJson(json, user);
    json += "}";
}

void UserStore::appendUserExportJson(String &json, const UserRecord *user) const {
    json += "{\"userName\":\"";
    appendJsonEscaped(json, user->userName, sizeof(user->userName));
    json += "\",\"addedAt\":";
    json += String((unsigned long)user->addedAt);
    json += ",\"isAdmin\":";
    json += user->isAdmin ? "true" : "false";
    json += ",\"editDevices\":";
    json += user->editDevices ? "true" : "false";
    json += ",\"addDevices\":";
    json += user->addDevices ? "true" : "false";
    json += ",\"removeDevices\":";
    json += user->removeDevices ? "true" : "false";
    json += ",\"editUsers\":";
    json += user->editUsers ? "true" : "false";
    json += ",\"editConsoles\":";
    json += user->editConsoles ? "true" : "false";
    json += ",\"monitor\":";
    json += user->monitor ? "true" : "false";
    json += ",\"controlDevices\":";
    json += user->controlDevices ? "true" : "false";
    json += ",\"isBlocked\":";
    json += user->isBlocked ? "true" : "false";
    json += ",\"theme\":\"";
    json += themeJsonId(user->theme);
    json += "\"";
    appendConsolesJson(json, user);
    json += ",\"passwordSalt\":\"";
    json += user->passwordSaltHex;
    json += "\",\"passwordHash\":\"";
    json += user->passwordHashHex;
    json += "\"}";
}

String UserStore::listPublicJson() const {
    String json = "[";
    bool first = true;
    for (const UserRecord *user = usersHead; user != nullptr; user = user->next) {
        if (!first) {
            json += ",";
        }
        first = false;
        appendUserPublicJson(json, user);
    }
    json += "]";
    return json;
}

String UserStore::listExportJson() const {
    String json = "[";
    bool first = true;
    for (const UserRecord *user = usersHead; user != nullptr; user = user->next) {
        if (!first) {
            json += ",";
        }
        first = false;
        appendUserExportJson(json, user);
    }
    json += "]";
    return json;
}

UserStore::SessionSlot *UserStore::findSession(const uint8_t token[AUTH_SESSION_TOKEN_LEN]) {
    for (int index = 0; index < AUTH_SESSION_MAX; index++) {
        if (sessions[index].used && !sessions[index].durableRemember
            && memcmp(sessions[index].token, token, AUTH_SESSION_TOKEN_LEN) == 0) {
            return &sessions[index];
        }
    }
    return nullptr;
}

UserStore::SessionSlot *UserStore::findDurableSession(const char *userName) {
    if (userName == nullptr) {
        return nullptr;
    }
    for (int index = 0; index < AUTH_SESSION_MAX; index++) {
        if (sessions[index].used && sessions[index].durableRemember
            && nameEquals(sessions[index].userName, userName)) {
            return &sessions[index];
        }
    }
    return nullptr;
}

UserStore::SessionSlot *UserStore::allocateSessionSlot() {
    for (int index = 0; index < AUTH_SESSION_MAX; index++) {
        if (!sessions[index].used) {
            return &sessions[index];
        }
    }
    return &sessions[0];
}

UserStore::SessionSlot *UserStore::rehydrateDurableSession(
    const UserRecord *user,
    uint32_t nowMs,
    uint32_t expiresAtUnix
) {
    if (user == nullptr) {
        return nullptr;
    }
    SessionSlot *slot = findDurableSession(user->userName);
    if (slot == nullptr) {
        slot = allocateSessionSlot();
    }
    memset(slot, 0, sizeof(*slot));
    strncpy(slot->userName, user->userName, sizeof(slot->userName) - 1);
    slot->lastActivityMs = nowMs;
    slot->maxIdleMs = AUTH_REMEMBER_MS;
    slot->expiresAtUnix = expiresAtUnix;
    slot->durableRemember = true;
    slot->used = true;
    return slot;
}

void UserStore::clearSessionsForUser(const char *userName) {
    if (userName == nullptr || userName[0] == '\0') {
        return;
    }
    for (int index = 0; index < AUTH_SESSION_MAX; index++) {
        if (sessions[index].used && nameEquals(sessions[index].userName, userName)) {
            memset(&sessions[index], 0, sizeof(sessions[index]));
        }
    }
}

bool UserStore::userGetsLongIdleSession(const UserRecord *user) {
    if (user == nullptr || user->isAdmin) {
        return false;
    }
    // Privileged operator roles keep the short idle unless Remember me is set.
    if (user->editDevices || user->addDevices || user->removeDevices
        || user->editUsers || user->editConsoles || user->controlDevices) {
        return false;
    }
    // Simple user (no roles) or monitor-only.
    return true;
}

bool UserStore::wallClockIsSane(time_t wallClock) {
    return wallClock >= AUTH_WALL_CLOCK_MIN;
}

bool UserStore::computeRememberMac(
    const UserRecord *user,
    uint32_t expiresAtUnix,
    uint8_t macOut[AUTH_REMEMBER_MAC_LEN]
) {
    if (user == nullptr || macOut == nullptr) {
        return false;
    }
    uint8_t passwordHash[USER_PASSWORD_HASH_LEN];
    if (!hexToBytes(user->passwordHashHex, passwordHash, sizeof(passwordHash))) {
        return false;
    }
    char message[USER_NAME_MAX + 16 + USER_PASSWORD_SALT_LEN * 2 + 8];
    const int messageLen = snprintf(
        message,
        sizeof(message),
        "%s|%08lx|%s",
        user->userName,
        (unsigned long)expiresAtUnix,
        user->passwordSaltHex
    );
    if (messageLen <= 0 || (size_t)messageLen >= sizeof(message)) {
        return false;
    }
    uint8_t fullMac[32];
    const mbedtls_md_info_t *mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (mdInfo == nullptr) {
        return false;
    }
    if (mbedtls_md_hmac(
            mdInfo,
            passwordHash,
            sizeof(passwordHash),
            (const unsigned char *)message,
            (size_t)messageLen,
            fullMac
        ) != 0) {
        return false;
    }
    memcpy(macOut, fullMac, AUTH_REMEMBER_MAC_LEN);
    return true;
}

bool UserStore::issueRememberProof(
    const UserRecord *user,
    uint32_t expiresAtUnix,
    char *cookieOut,
    size_t cookieOutSize
) {
    if (user == nullptr || cookieOut == nullptr || cookieOutSize < 16) {
        return false;
    }
    uint8_t mac[AUTH_REMEMBER_MAC_LEN];
    if (!computeRememberMac(user, expiresAtUnix, mac)) {
        return false;
    }
    char userNameHex[USER_NAME_MAX * 2 + 1];
    bytesToHex((const uint8_t *)user->userName, strlen(user->userName), userNameHex);
    char macHex[AUTH_REMEMBER_MAC_LEN * 2 + 1];
    bytesToHex(mac, sizeof(mac), macHex);
    const int written = snprintf(
        cookieOut,
        cookieOutSize,
        "%s%s.%08lx.%s",
        AUTH_REMEMBER_COOKIE_PREFIX,
        userNameHex,
        (unsigned long)expiresAtUnix,
        macHex
    );
    return written > 0 && (size_t)written < cookieOutSize;
}

bool UserStore::parseRememberProof(
    const char *cookieValue,
    char *userNameOut,
    size_t userNameOutSize,
    uint32_t *expiresAtUnixOut,
    uint8_t macOut[AUTH_REMEMBER_MAC_LEN]
) {
    if (cookieValue == nullptr || userNameOut == nullptr || userNameOutSize == 0
        || expiresAtUnixOut == nullptr || macOut == nullptr) {
        return false;
    }
    const size_t prefixLen = strlen(AUTH_REMEMBER_COOKIE_PREFIX);
    if (strncmp(cookieValue, AUTH_REMEMBER_COOKIE_PREFIX, prefixLen) != 0) {
        return false;
    }
    const char *cursor = cookieValue + prefixLen;
    const char *firstDot = strchr(cursor, '.');
    if (firstDot == nullptr) {
        return false;
    }
    const char *secondDot = strchr(firstDot + 1, '.');
    if (secondDot == nullptr) {
        return false;
    }
    const size_t nameHexLen = (size_t)(firstDot - cursor);
    if (nameHexLen == 0 || (nameHexLen % 2) != 0 || nameHexLen / 2 >= userNameOutSize) {
        return false;
    }
    if ((size_t)(secondDot - (firstDot + 1)) != 8) {
        return false;
    }
    if (strlen(secondDot + 1) != AUTH_REMEMBER_MAC_LEN * 2) {
        return false;
    }
    uint8_t nameBytes[USER_NAME_MAX];
    if (!hexToBytes(cursor, nameBytes, nameHexLen / 2)) {
        return false;
    }
    memcpy(userNameOut, nameBytes, nameHexLen / 2);
    userNameOut[nameHexLen / 2] = '\0';
    unsigned long expiryValue = 0;
    if (sscanf(firstDot + 1, "%8lx", &expiryValue) != 1) {
        return false;
    }
    *expiresAtUnixOut = (uint32_t)expiryValue;
    if (!hexToBytes(secondDot + 1, macOut, AUTH_REMEMBER_MAC_LEN)) {
        return false;
    }
    return true;
}

bool UserStore::verifyRememberProof(
    const UserRecord *user,
    uint32_t expiresAtUnix,
    const uint8_t mac[AUTH_REMEMBER_MAC_LEN]
) {
    if (user == nullptr || mac == nullptr || user->isBlocked) {
        return false;
    }
    if (!userGetsLongIdleSession(user)) {
        return false;
    }
    const time_t wallClock = time(nullptr);
    if (!wallClockIsSane(wallClock) || (uint32_t)wallClock > expiresAtUnix) {
        return false;
    }
    uint8_t expectedMac[AUTH_REMEMBER_MAC_LEN];
    if (!computeRememberMac(user, expiresAtUnix, expectedMac)) {
        return false;
    }
    return hashesEqual(expectedMac, mac, AUTH_REMEMBER_MAC_LEN);
}

bool UserStore::extractCookieValue(const char *cookieHeader, char *valueOut, size_t valueOutSize) {
    if (cookieHeader == nullptr || valueOut == nullptr || valueOutSize == 0) {
        return false;
    }
    const char *found = strstr(cookieHeader, AUTH_COOKIE_NAME "=");
    if (found == nullptr) {
        return false;
    }
    found += strlen(AUTH_COOKIE_NAME "=");
    size_t valueIndex = 0;
    while (valueIndex + 1 < valueOutSize && found[valueIndex] != '\0' && found[valueIndex] != ';') {
        valueOut[valueIndex] = found[valueIndex];
        valueIndex++;
    }
    valueOut[valueIndex] = '\0';
    return valueIndex > 0;
}

bool UserStore::isOpaqueSessionToken(const char *cookieValue) {
    if (cookieValue == nullptr) {
        return false;
    }
    const size_t length = strlen(cookieValue);
    if (length != AUTH_SESSION_TOKEN_LEN * 2) {
        return false;
    }
    for (size_t index = 0; index < length; index++) {
        if (!isxdigit((unsigned char)cookieValue[index])) {
            return false;
        }
    }
    return true;
}

bool UserStore::startSession(
    const UserRecord *user,
    bool rememberMe,
    uint32_t nowMs,
    char *tokenHex,
    size_t tokenHexSize,
    uint32_t *maxAgeSec
) {
    if (user == nullptr || user->isBlocked || tokenHex == nullptr) {
        return false;
    }
    const bool durableRemember =
        rememberMe && userGetsLongIdleSession(user) && wallClockIsSane(time(nullptr));
    if (durableRemember) {
        if (tokenHexSize < AUTH_SESSION_COOKIE_MAX) {
            return false;
        }
        const time_t wallClock = time(nullptr);
        const uint32_t expiresAtUnix =
            (uint32_t)wallClock + (uint32_t)(AUTH_REMEMBER_MS / 1000UL);
        if (!issueRememberProof(user, expiresAtUnix, tokenHex, tokenHexSize)) {
            return false;
        }
        if (rehydrateDurableSession(user, nowMs, expiresAtUnix) == nullptr) {
            return false;
        }
        if (maxAgeSec != nullptr) {
            *maxAgeSec = AUTH_REMEMBER_MS / 1000UL;
        }
        return true;
    }
    if (tokenHexSize < AUTH_SESSION_TOKEN_LEN * 2 + 1) {
        return false;
    }
    SessionSlot *slot = allocateSessionSlot();
    memset(slot, 0, sizeof(*slot));
    esp_fill_random(slot->token, sizeof(slot->token));
    strncpy(slot->userName, user->userName, sizeof(slot->userName) - 1);
    slot->lastActivityMs = nowMs;
    slot->durableRemember = false;
    slot->expiresAtUnix = 0;
    if (user->isAdmin) {
        slot->maxIdleMs = AUTH_ADMIN_IDLE_MS;
        if (maxAgeSec != nullptr) {
            *maxAgeSec = 0;
        }
    } else if (rememberMe || userGetsLongIdleSession(user)) {
        slot->maxIdleMs = AUTH_REMEMBER_MS;
        if (maxAgeSec != nullptr) {
            *maxAgeSec = AUTH_REMEMBER_MS / 1000UL;
        }
    } else {
        slot->maxIdleMs = AUTH_SESSION_MS;
        if (maxAgeSec != nullptr) {
            *maxAgeSec = 0;
        }
    }
    slot->used = true;
    bytesToHex(slot->token, sizeof(slot->token), tokenHex);
    return true;
}

void UserStore::endSession(const char *tokenHex) {
    if (tokenHex == nullptr || tokenHex[0] == '\0') {
        return;
    }
    if (strncmp(tokenHex, AUTH_REMEMBER_COOKIE_PREFIX, strlen(AUTH_REMEMBER_COOKIE_PREFIX)) == 0) {
        char userName[USER_NAME_MAX];
        uint32_t expiresAtUnix = 0;
        uint8_t mac[AUTH_REMEMBER_MAC_LEN];
        if (parseRememberProof(tokenHex, userName, sizeof(userName), &expiresAtUnix, mac)) {
            clearSessionsForUser(userName);
        }
        return;
    }
    uint8_t token[AUTH_SESSION_TOKEN_LEN];
    if (!hexToBytes(tokenHex, token, sizeof(token))) {
        return;
    }
    SessionSlot *slot = findSession(token);
    if (slot != nullptr) {
        memset(slot, 0, sizeof(*slot));
    }
}

void UserStore::touchSession(const char *tokenHex, uint32_t nowMs) {
    if (tokenHex == nullptr || tokenHex[0] == '\0') {
        return;
    }
    if (strncmp(tokenHex, AUTH_REMEMBER_COOKIE_PREFIX, strlen(AUTH_REMEMBER_COOKIE_PREFIX)) == 0) {
        char userName[USER_NAME_MAX];
        uint32_t expiresAtUnix = 0;
        uint8_t mac[AUTH_REMEMBER_MAC_LEN];
        if (!parseRememberProof(tokenHex, userName, sizeof(userName), &expiresAtUnix, mac)) {
            return;
        }
        SessionSlot *slot = findDurableSession(userName);
        if (slot != nullptr) {
            slot->lastActivityMs = nowMs;
        }
        return;
    }
    uint8_t token[AUTH_SESSION_TOKEN_LEN];
    if (!hexToBytes(tokenHex, token, sizeof(token))) {
        return;
    }
    SessionSlot *slot = findSession(token);
    if (slot != nullptr) {
        slot->lastActivityMs = nowMs;
    }
}

const UserRecord *UserStore::sessionUser(
    const char *cookieHeader,
    uint32_t nowMs,
    char *tokenHexOut,
    size_t tokenHexOutSize,
    bool touchActivity
) {
    char cookieValue[AUTH_SESSION_COOKIE_MAX];
    if (!extractCookieValue(cookieHeader, cookieValue, sizeof(cookieValue))) {
        return nullptr;
    }
    if (strncmp(cookieValue, AUTH_REMEMBER_COOKIE_PREFIX, strlen(AUTH_REMEMBER_COOKIE_PREFIX)) == 0) {
        char userName[USER_NAME_MAX];
        uint32_t expiresAtUnix = 0;
        uint8_t mac[AUTH_REMEMBER_MAC_LEN];
        if (!parseRememberProof(cookieValue, userName, sizeof(userName), &expiresAtUnix, mac)) {
            return nullptr;
        }
        const UserRecord *user = findByName(userName);
        if (user == nullptr || !verifyRememberProof(user, expiresAtUnix, mac)) {
            return nullptr;
        }
        SessionSlot *slot = findDurableSession(userName);
        if (slot == nullptr) {
            slot = rehydrateDurableSession(user, nowMs, expiresAtUnix);
        }
        if (slot == nullptr) {
            return nullptr;
        }
        if ((uint32_t)(nowMs - slot->lastActivityMs) > slot->maxIdleMs) {
            memset(slot, 0, sizeof(*slot));
            return nullptr;
        }
        if (touchActivity) {
            slot->lastActivityMs = nowMs;
        }
        if (tokenHexOut != nullptr && tokenHexOutSize > 0) {
            strncpy(tokenHexOut, cookieValue, tokenHexOutSize - 1);
            tokenHexOut[tokenHexOutSize - 1] = '\0';
        }
        return user;
    }
    if (!isOpaqueSessionToken(cookieValue)) {
        return nullptr;
    }
    uint8_t token[AUTH_SESSION_TOKEN_LEN];
    if (!hexToBytes(cookieValue, token, sizeof(token))) {
        return nullptr;
    }
    SessionSlot *slot = findSession(token);
    if (slot == nullptr) {
        return nullptr;
    }
    if ((uint32_t)(nowMs - slot->lastActivityMs) > slot->maxIdleMs) {
        memset(slot, 0, sizeof(*slot));
        return nullptr;
    }
    const UserRecord *user = findByName(slot->userName);
    if (user == nullptr || user->isBlocked) {
        memset(slot, 0, sizeof(*slot));
        return nullptr;
    }
    if (touchActivity) {
        slot->lastActivityMs = nowMs;
    }
    if (tokenHexOut != nullptr && tokenHexOutSize > 0) {
        strncpy(tokenHexOut, cookieValue, tokenHexOutSize - 1);
        tokenHexOut[tokenHexOutSize - 1] = '\0';
    }
    return user;
}
