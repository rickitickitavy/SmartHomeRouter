#include "InterconnectStore.h"
#include "JsonField.h"
#include "Logger.h"

#include <LittleFS.h>
#include <string.h>

namespace {

void clearMac(uint8_t mac[6]) {
    memset(mac, 0, 6);
}

bool macEqual(const uint8_t a[6], const uint8_t b[6]) {
    return memcmp(a, b, 6) == 0;
}

bool macIsZero(const uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0) {
            return false;
        }
    }
    return true;
}

bool parseMac(const char *text, uint8_t mac[6]) {
    return InterconnectStore::parseMacText(text, mac);
}

void formatMac(const uint8_t mac[6], char out[18]) {
    snprintf(
        out,
        18,
        "%02x:%02x:%02x:%02x:%02x:%02x",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]
    );
}

void appendHex(PsramJsonBuffer &json, const uint8_t *bytes, size_t len) {
    static const char *hex = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        json += hex[(bytes[i] >> 4) & 0xF];
        json += hex[bytes[i] & 0xF];
    }
}

bool parseHex(const char *text, uint8_t *out, size_t outLen) {
    if (text == nullptr || out == nullptr || strlen(text) != outLen * 2) {
        return false;
    }
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        return -1;
    };
    for (size_t i = 0; i < outLen; i++) {
        const int hi = nibble(text[i * 2]);
        const int lo = nibble(text[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

}  // namespace

InterconnectStore::InterconnectStore() {
    memset(&settingsState, 0, sizeof(settingsState));
    settingsState.role = InterconnectRoleMain;
    settingsState.port = INTERCONNECT_PORT_DEFAULT;
    settingsState.checkAliveSec = INTERCONNECT_CHECK_ALIVE_DEFAULT_SEC;
    strncpy(settingsState.routerName, "router", INTERCONNECT_NAME_MAX);
    for (int i = 0; i < INTERCONNECT_MAX_REPEATERS; i++) {
        freePeerSlot(&peers[i]);
    }
}

InterconnectStore::~InterconnectStore() = default;

bool InterconnectStore::begin(bool persistToLittleFs) {
    persistEnabled = persistToLittleFs;
    if (!persistEnabled) {
        return true;
    }
    if (!loadFromFile()) {
        LOGGER.info("Interconnect store empty; using defaults");
    }
    return true;
}

void InterconnectStore::requestPersist() {
    if (!persistEnabled) {
        return;
    }
    persistPending = true;
    persistDueMs = millis() + 250;
}

void InterconnectStore::persistIfDue() {
    if (!persistPending || !persistEnabled) {
        return;
    }
    if ((int32_t)(millis() - persistDueMs) < 0) {
        return;
    }
    persistPending = false;
    if (!saveToFile()) {
        LOGGER.warning("Interconnect persist failed");
        persistPending = true;
        persistDueMs = millis() + 1000;
    }
}

int InterconnectStore::peerCount() const {
    return usedPeerSlots();
}

int InterconnectStore::usedPeerSlots() const {
    int count = 0;
    for (int i = 0; i < INTERCONNECT_MAX_REPEATERS; i++) {
        if (peers[i].used) {
            count++;
        }
    }
    return count;
}

InterconnectPeer *InterconnectStore::firstPeer() {
    for (int i = 0; i < INTERCONNECT_MAX_REPEATERS; i++) {
        if (peers[i].used) {
            return &peers[i];
        }
    }
    return nullptr;
}

const InterconnectPeer *InterconnectStore::firstPeer() const {
    return const_cast<InterconnectStore *>(this)->firstPeer();
}

InterconnectPeer *InterconnectStore::peerAt(int index) {
    if (index < 0 || index >= INTERCONNECT_MAX_REPEATERS) {
        return nullptr;
    }
    return peers[index].used ? &peers[index] : nullptr;
}

const InterconnectPeer *InterconnectStore::peerAt(int index) const {
    return const_cast<InterconnectStore *>(this)->peerAt(index);
}

bool InterconnectStore::parseMacText(const char *text, uint8_t mac[6]) {
    if (text == nullptr || mac == nullptr) {
        return false;
    }
    unsigned values[6] = {0};
    if (sscanf(
            text,
            "%02x:%02x:%02x:%02x:%02x:%02x",
            &values[0],
            &values[1],
            &values[2],
            &values[3],
            &values[4],
            &values[5]
        )
        != 6) {
        return false;
    }
    for (int i = 0; i < 6; i++) {
        mac[i] = (uint8_t)values[i];
    }
    return true;
}

InterconnectPeer *InterconnectStore::findByMac(const uint8_t mac[6]) {
    if (mac == nullptr) {
        return nullptr;
    }
    for (int i = 0; i < INTERCONNECT_MAX_REPEATERS; i++) {
        if (peers[i].used && macEqual(peers[i].mac, mac)) {
            return &peers[i];
        }
    }
    return nullptr;
}

InterconnectPeer *InterconnectStore::findByMacText(const char *macText) {
    uint8_t mac[6];
    if (!parseMacText(macText, mac)) {
        return nullptr;
    }
    return findByMac(mac);
}

InterconnectPeer *InterconnectStore::findByRouterId(uint32_t routerId) {
    if (routerId == 0) {
        return nullptr;
    }
    for (int i = 0; i < INTERCONNECT_MAX_REPEATERS; i++) {
        if (peers[i].used && peers[i].routerId == routerId) {
            return &peers[i];
        }
    }
    return nullptr;
}

InterconnectPeer *InterconnectStore::findByIp(const char *ip) {
    if (ip == nullptr || ip[0] == '\0') {
        return nullptr;
    }
    for (int i = 0; i < INTERCONNECT_MAX_REPEATERS; i++) {
        if (peers[i].used && strcmp(peers[i].ip, ip) == 0) {
            return &peers[i];
        }
    }
    return nullptr;
}

void InterconnectStore::freePeerSlot(InterconnectPeer *peer) {
    if (peer == nullptr) {
        return;
    }
    memset(peer, 0, sizeof(*peer));
}

bool InterconnectStore::setLocalIdentity(
    const char *name,
    InterconnectRole role,
    uint16_t port,
    uint16_t checkAliveSec
) {
    if (name == nullptr || name[0] == '\0' || strlen(name) > INTERCONNECT_NAME_MAX) {
        return false;
    }
    if (port == 0) {
        return false;
    }
    if (checkAliveSec < INTERCONNECT_CHECK_ALIVE_MIN_SEC
        || checkAliveSec > INTERCONNECT_CHECK_ALIVE_MAX_SEC) {
        return false;
    }
    strncpy(settingsState.routerName, name, INTERCONNECT_NAME_MAX);
    settingsState.routerName[INTERCONNECT_NAME_MAX] = '\0';
    settingsState.role = role;
    settingsState.port = port;
    settingsState.checkAliveSec = checkAliveSec;
    requestPersist();
    return true;
}

void InterconnectStore::clearPeers() {
    for (int i = 0; i < INTERCONNECT_MAX_REPEATERS; i++) {
        freePeerSlot(&peers[i]);
    }
    settingsState.hasMain = false;
    clearMac(settingsState.mainMac);
    settingsState.mainName[0] = '\0';
    settingsState.mainIp[0] = '\0';
    settingsState.mainPort = 0;
    settingsState.mainOnline = false;
    settingsState.mainLastAliveMs = 0;
    memset(settingsState.mainSecret, 0, sizeof(settingsState.mainSecret));
    requestPersist();
}

bool InterconnectStore::setMainPeer(
    const char *name,
    const uint8_t mac[6],
    const char *ip,
    uint16_t port,
    const uint8_t secret[INTERCONNECT_SECRET_LEN]
) {
    if (name == nullptr || mac == nullptr || secret == nullptr || macIsZero(mac) || port == 0) {
        return false;
    }
    if (strlen(name) > INTERCONNECT_NAME_MAX) {
        return false;
    }
    strncpy(settingsState.mainName, name, INTERCONNECT_NAME_MAX);
    settingsState.mainName[INTERCONNECT_NAME_MAX] = '\0';
    memcpy(settingsState.mainMac, mac, 6);
    settingsState.mainIp[0] = '\0';
    if (ip != nullptr) {
        strncpy(settingsState.mainIp, ip, sizeof(settingsState.mainIp) - 1);
    }
    settingsState.mainPort = port;
    memcpy(settingsState.mainSecret, secret, INTERCONNECT_SECRET_LEN);
    settingsState.hasMain = true;
    settingsState.mainOnline = true;
    settingsState.mainLastAliveMs = millis();
    requestPersist();
    return true;
}

InterconnectPeer *InterconnectStore::addPeer(
    const char *name,
    const uint8_t mac[6],
    const char *ip,
    uint16_t port,
    uint32_t routerId,
    uint8_t zigbeeChannel,
    const uint8_t secret[INTERCONNECT_SECRET_LEN]
) {
    if (name == nullptr || mac == nullptr || secret == nullptr || macIsZero(mac) || routerId == 0) {
        return nullptr;
    }
    if (usedPeerSlots() >= INTERCONNECT_MAX_REPEATERS) {
        return nullptr;
    }
    if (findByMac(mac) != nullptr || findByRouterId(routerId) != nullptr) {
        return nullptr;
    }
    InterconnectPeer *slot = nullptr;
    for (int i = 0; i < INTERCONNECT_MAX_REPEATERS; i++) {
        if (!peers[i].used) {
            slot = &peers[i];
            break;
        }
    }
    if (slot == nullptr) {
        return nullptr;
    }
    freePeerSlot(slot);
    strncpy(slot->name, name, INTERCONNECT_NAME_MAX);
    slot->name[INTERCONNECT_NAME_MAX] = '\0';
    memcpy(slot->mac, mac, 6);
    if (ip != nullptr) {
        strncpy(slot->ip, ip, sizeof(slot->ip) - 1);
    }
    slot->port = port == 0 ? INTERCONNECT_PORT_DEFAULT : port;
    slot->routerId = routerId;
    slot->zigbeeChannel = zigbeeChannel;
    memcpy(slot->secret, secret, INTERCONNECT_SECRET_LEN);
    slot->hasSecret = true;
    slot->used = true;
    requestPersist();
    return slot;
}

bool InterconnectStore::removePeerByMac(const uint8_t mac[6]) {
    InterconnectPeer *peer = findByMac(mac);
    if (peer == nullptr) {
        return false;
    }
    freePeerSlot(peer);
    requestPersist();
    return true;
}

bool InterconnectStore::replacePeerEndpoint(
    uint32_t routerId,
    const char *name,
    const uint8_t mac[6],
    const char *ip,
    uint16_t port,
    const uint8_t secret[INTERCONNECT_SECRET_LEN]
) {
    if (routerId == 0 || name == nullptr || mac == nullptr || secret == nullptr || macIsZero(mac)) {
        return false;
    }
    InterconnectPeer *peer = findByRouterId(routerId);
    if (peer == nullptr || !peer->used) {
        return false;
    }
    InterconnectPeer *macOwner = findByMac(mac);
    if (macOwner != nullptr && macOwner != peer) {
        return false;
    }
    strncpy(peer->name, name, INTERCONNECT_NAME_MAX);
    peer->name[INTERCONNECT_NAME_MAX] = '\0';
    memcpy(peer->mac, mac, 6);
    if (ip != nullptr) {
        strncpy(peer->ip, ip, sizeof(peer->ip) - 1);
        peer->ip[sizeof(peer->ip) - 1] = '\0';
    } else {
        peer->ip[0] = '\0';
    }
    peer->port = port == 0 ? INTERCONNECT_PORT_DEFAULT : port;
    memcpy(peer->secret, secret, INTERCONNECT_SECRET_LEN);
    peer->hasSecret = true;
    peer->online = true;
    peer->lastAliveMs = millis();
    requestPersist();
    return true;
}

uint32_t InterconnectStore::allocateRouterId() const {
    uint32_t candidate = 1;
    while (candidate != 0) {
        bool used = false;
        for (int i = 0; i < INTERCONNECT_MAX_REPEATERS; i++) {
            if (peers[i].used && peers[i].routerId == candidate) {
                used = true;
                break;
            }
        }
        if (!used) {
            return candidate;
        }
        candidate++;
    }
    return 0;
}

uint8_t InterconnectStore::allocateZigbeeChannel(uint8_t mainChannel) const {
    bool taken[27] = {};
    if (mainChannel >= 11 && mainChannel <= 26) {
        taken[mainChannel] = true;
    }
    for (int i = 0; i < INTERCONNECT_MAX_REPEATERS; i++) {
        if (!peers[i].used) {
            continue;
        }
        const uint8_t channel = peers[i].zigbeeChannel;
        if (channel >= 11 && channel <= 26) {
            taken[channel] = true;
        }
    }
    for (uint8_t channel = 11; channel <= 26; channel++) {
        if (!taken[channel]) {
            return channel;
        }
    }
    return 0;
}

PsramJsonBuffer InterconnectStore::statusJson(uint32_t pairingWaitRemainingMs) const {
    PsramJsonBuffer json;
    json += "{\"routerName\":\"";
    appendJsonEscaped(json, settingsState.routerName, strlen(settingsState.routerName) + 1);
    json += "\",\"role\":\"";
    json += settingsState.role == InterconnectRoleRepeater ? "Repeater" : "Main";
    json += "\",\"port\":";
    json += String(settingsState.port);
    json += ",\"checkAliveSec\":";
    json += String(settingsState.checkAliveSec);
    json += ",\"pairingWaitRemainingMs\":";
    json += String((unsigned long)pairingWaitRemainingMs);
    json += ",\"pairingWaitActive\":";
    json += pairingWaitRemainingMs > 0 ? "true" : "false";
    if (settingsState.hasMain) {
        char macText[18];
        formatMac(settingsState.mainMac, macText);
        json += ",\"main\":{\"name\":\"";
        appendJsonEscaped(json, settingsState.mainName, strlen(settingsState.mainName) + 1);
        json += "\",\"mac\":\"";
        json += macText;
        json += "\",\"ip\":\"";
        appendJsonEscaped(json, settingsState.mainIp, strlen(settingsState.mainIp) + 1);
        json += "\",\"port\":";
        json += String(settingsState.mainPort);
        json += ",\"online\":";
        json += settingsState.mainOnline ? "true" : "false";
        json += "}";
    } else {
        json += ",\"main\":null";
    }
    json += ",\"peers\":[";
    bool first = true;
    for (int i = 0; i < INTERCONNECT_MAX_REPEATERS; i++) {
        if (!peers[i].used) {
            continue;
        }
        if (!first) {
            json += ",";
        }
        first = false;
        char macText[18];
        formatMac(peers[i].mac, macText);
        json += "{\"name\":\"";
        appendJsonEscaped(json, peers[i].name, strlen(peers[i].name) + 1);
        json += "\",\"mac\":\"";
        json += macText;
        json += "\",\"ip\":\"";
        appendJsonEscaped(json, peers[i].ip, strlen(peers[i].ip) + 1);
        json += "\",\"port\":";
        json += String(peers[i].port);
        json += ",\"routerId\":";
        json += String((unsigned long)peers[i].routerId);
        json += ",\"zigbeeChannel\":";
        json += String(peers[i].zigbeeChannel);
        json += ",\"hostVersion\":\"";
        appendJsonEscaped(json, peers[i].hostVersion, strlen(peers[i].hostVersion) + 1);
        json += "\",\"online\":";
        json += peers[i].online ? "true" : "false";
        json += ",\"packetsIn\":";
        json += String((unsigned long)peers[i].packetsIn);
        json += ",\"packetsOut\":";
        json += String((unsigned long)peers[i].packetsOut);
        json += ",\"deviceCount\":";
        json += String(peers[i].deviceCount);
        json += "}";
    }
    json += "]}";
    return json;
}

PsramJsonBuffer InterconnectStore::exportJson() const {
    PsramJsonBuffer json;
    json += "{\"routerName\":\"";
    appendJsonEscaped(json, settingsState.routerName, strlen(settingsState.routerName) + 1);
    json += "\",\"role\":\"";
    json += settingsState.role == InterconnectRoleRepeater ? "Repeater" : "Main";
    json += "\",\"port\":";
    json += String(settingsState.port);
    json += ",\"checkAliveSec\":";
    json += String(settingsState.checkAliveSec);
    if (settingsState.hasMain) {
        char macText[18];
        formatMac(settingsState.mainMac, macText);
        json += ",\"main\":{\"name\":\"";
        appendJsonEscaped(json, settingsState.mainName, strlen(settingsState.mainName) + 1);
        json += "\",\"mac\":\"";
        json += macText;
        json += "\",\"ip\":\"";
        appendJsonEscaped(json, settingsState.mainIp, strlen(settingsState.mainIp) + 1);
        json += "\",\"port\":";
        json += String(settingsState.mainPort);
        json += ",\"secret\":\"";
        appendHex(json, settingsState.mainSecret, INTERCONNECT_SECRET_LEN);
        json += "\"}";
    }
    json += ",\"peers\":[";
    bool first = true;
    for (int i = 0; i < INTERCONNECT_MAX_REPEATERS; i++) {
        if (!peers[i].used) {
            continue;
        }
        if (!first) {
            json += ",";
        }
        first = false;
        char macText[18];
        formatMac(peers[i].mac, macText);
        json += "{\"name\":\"";
        appendJsonEscaped(json, peers[i].name, strlen(peers[i].name) + 1);
        json += "\",\"mac\":\"";
        json += macText;
        json += "\",\"ip\":\"";
        appendJsonEscaped(json, peers[i].ip, strlen(peers[i].ip) + 1);
        json += "\",\"port\":";
        json += String(peers[i].port);
        json += ",\"routerId\":";
        json += String((unsigned long)peers[i].routerId);
        json += ",\"zigbeeChannel\":";
        json += String(peers[i].zigbeeChannel);
        json += ",\"hostVersion\":\"";
        appendJsonEscaped(json, peers[i].hostVersion, strlen(peers[i].hostVersion) + 1);
        json += "\",\"secret\":\"";
        appendHex(json, peers[i].secret, INTERCONNECT_SECRET_LEN);
        json += "\"}";
    }
    json += "]}";
    return json;
}

bool InterconnectStore::importJson(const char *json, bool keepExistingMacAuthoritative) {
    if (json == nullptr) {
        return false;
    }
    String name;
    String roleText;
    int port = INTERCONNECT_PORT_DEFAULT;
    int checkAlive = INTERCONNECT_CHECK_ALIVE_DEFAULT_SEC;
    if (!extractJsonString(json, "routerName", name) || name.length() == 0
        || name.length() > INTERCONNECT_NAME_MAX) {
        return false;
    }
    if (!extractJsonString(json, "role", roleText)) {
        roleText = "Main";
    }
    extractJsonInt(json, "port", port);
    extractJsonInt(json, "checkAliveSec", checkAlive);
    if (port <= 0 || port > 65535) {
        return false;
    }
    if (checkAlive < INTERCONNECT_CHECK_ALIVE_MIN_SEC
        || checkAlive > INTERCONNECT_CHECK_ALIVE_MAX_SEC) {
        return false;
    }
    InterconnectRole role = InterconnectRoleMain;
    if (roleText.equalsIgnoreCase("Repeater")) {
        role = InterconnectRoleRepeater;
    }

    InterconnectPeer keep[INTERCONNECT_MAX_REPEATERS];
    int keepCount = 0;
    if (keepExistingMacAuthoritative) {
        for (int i = 0; i < INTERCONNECT_MAX_REPEATERS; i++) {
            if (peers[i].used) {
                keep[keepCount++] = peers[i];
            }
        }
    }

    clearPeers();
    setLocalIdentity(name.c_str(), role, (uint16_t)port, (uint16_t)checkAlive);

    String peersSlice;
    if (extractJsonKeyedSlice(json, "peers", '[', peersSlice)) {
        const char *cursor = peersSlice.c_str();
        while (*cursor != '\0') {
            while (*cursor != '\0' && *cursor != '{') {
                cursor++;
            }
            if (*cursor != '{') {
                break;
            }
            const char *objectEnd = nullptr;
            int depth = 0;
            for (const char *p = cursor; *p != '\0'; p++) {
                if (*p == '{') {
                    depth++;
                } else if (*p == '}') {
                    depth--;
                    if (depth == 0) {
                        objectEnd = p;
                        break;
                    }
                }
            }
            if (objectEnd == nullptr) {
                break;
            }
            String object = String(cursor).substring(0, (int)(objectEnd - cursor + 1));
            String peerName;
            String macText;
            String ipText;
            String secretText;
            int peerPort = INTERCONNECT_PORT_DEFAULT;
            int routerId = 0;
            int channel = 11;
            if (extractJsonString(object.c_str(), "name", peerName)
                && extractJsonString(object.c_str(), "mac", macText)
                && extractJsonString(object.c_str(), "secret", secretText)) {
                extractJsonInt(object.c_str(), "port", peerPort);
                extractJsonInt(object.c_str(), "routerId", routerId);
                extractJsonInt(object.c_str(), "zigbeeChannel", channel);
                extractJsonString(object.c_str(), "ip", ipText);
                String hostVersion;
                extractJsonString(object.c_str(), "hostVersion", hostVersion);
                uint8_t mac[6];
                uint8_t secret[INTERCONNECT_SECRET_LEN];
                if (parseMac(macText.c_str(), mac) && parseHex(secretText.c_str(), secret, INTERCONNECT_SECRET_LEN)) {
                    bool skipped = false;
                    if (keepExistingMacAuthoritative) {
                        for (int i = 0; i < keepCount; i++) {
                            if (macEqual(keep[i].mac, mac)) {
                                InterconnectPeer *kept = addPeer(
                                    keep[i].name,
                                    keep[i].mac,
                                    keep[i].ip,
                                    keep[i].port,
                                    keep[i].routerId,
                                    keep[i].zigbeeChannel,
                                    keep[i].secret
                                );
                                if (kept != nullptr) {
                                    strncpy(
                                        kept->hostVersion,
                                        keep[i].hostVersion,
                                        INTERCONNECT_FW_VERSION_MAX
                                    );
                                    kept->hostVersion[INTERCONNECT_FW_VERSION_MAX] = '\0';
                                }
                                skipped = true;
                                break;
                            }
                        }
                    }
                    if (!skipped && routerId > 0) {
                        InterconnectPeer *added = addPeer(
                            peerName.c_str(),
                            mac,
                            ipText.c_str(),
                            (uint16_t)peerPort,
                            (uint32_t)routerId,
                            (uint8_t)channel,
                            secret
                        );
                        if (added != nullptr && hostVersion.length() > 0) {
                            strncpy(
                                added->hostVersion,
                                hostVersion.c_str(),
                                INTERCONNECT_FW_VERSION_MAX
                            );
                            added->hostVersion[INTERCONNECT_FW_VERSION_MAX] = '\0';
                        }
                    }
                }
            }
            cursor = objectEnd + 1;
        }
    }

    if (keepExistingMacAuthoritative) {
        for (int i = 0; i < keepCount; i++) {
            if (findByMac(keep[i].mac) == nullptr) {
                InterconnectPeer *kept = addPeer(
                    keep[i].name,
                    keep[i].mac,
                    keep[i].ip,
                    keep[i].port,
                    keep[i].routerId,
                    keep[i].zigbeeChannel,
                    keep[i].secret
                );
                if (kept != nullptr) {
                    strncpy(kept->hostVersion, keep[i].hostVersion, INTERCONNECT_FW_VERSION_MAX);
                    kept->hostVersion[INTERCONNECT_FW_VERSION_MAX] = '\0';
                }
            }
        }
    }

    String mainSlice;
    if (extractJsonKeyedSlice(json, "main", '{', mainSlice)) {
        String mainName;
        String macText;
        String ipText;
        String secretText;
        int mainPort = INTERCONNECT_PORT_DEFAULT;
        if (extractJsonString(mainSlice.c_str(), "name", mainName)
            && extractJsonString(mainSlice.c_str(), "mac", macText)
            && extractJsonString(mainSlice.c_str(), "secret", secretText)) {
            extractJsonInt(mainSlice.c_str(), "port", mainPort);
            extractJsonString(mainSlice.c_str(), "ip", ipText);
            if (parseMac(macText.c_str(), settingsState.mainMac)
                && parseHex(secretText.c_str(), settingsState.mainSecret, INTERCONNECT_SECRET_LEN)) {
                strncpy(settingsState.mainName, mainName.c_str(), INTERCONNECT_NAME_MAX);
                strncpy(settingsState.mainIp, ipText.c_str(), sizeof(settingsState.mainIp) - 1);
                settingsState.mainPort = (uint16_t)mainPort;
                settingsState.hasMain = true;
            }
        }
    }

    requestPersist();
    return true;
}

bool InterconnectStore::loadFromFile(const char *path) {
    if (path == nullptr) {
        return false;
    }
    File file = LittleFS.open(path, "r");
    if (!file) {
        return false;
    }
    PsramJsonBuffer json;
    const bool loaded = json.assignFromFile(file);
    file.close();
    if (!loaded || !json.ok()) {
        return false;
    }
    return importJson(json.c_str(), false);
}

bool InterconnectStore::saveToFile(const char *path) {
    if (path == nullptr) {
        return false;
    }
    PsramJsonBuffer json = exportJson();
    if (!json.ok()) {
        return false;
    }
    File file = LittleFS.open(path, "w");
    if (!file) {
        return false;
    }
    const size_t written = file.print(json.c_str());
    file.close();
    return written > 0;
}
