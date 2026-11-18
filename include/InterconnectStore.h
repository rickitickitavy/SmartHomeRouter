#pragma once

#include "Defines.h"
#include "DeviceTopicMap.h"
#include "PsramJsonBuffer.h"
#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

enum InterconnectRole : uint8_t {
    InterconnectRoleMain = 0,
    InterconnectRoleRepeater = 1
};

struct InterconnectPeer {
    char name[INTERCONNECT_NAME_MAX + 1];
    uint8_t mac[6];
    char ip[16];
    uint16_t port;
    uint32_t routerId;
    uint8_t zigbeeChannel;
    char hostVersion[INTERCONNECT_FW_VERSION_MAX + 1];
    uint8_t secret[INTERCONNECT_SECRET_LEN];
    bool hasSecret;
    // Runtime
    bool online;
    uint32_t packetsIn;
    uint32_t packetsOut;
    uint32_t lastAliveMs;
    uint16_t deviceCount;
    bool used;
};

struct InterconnectSettings {
    char routerName[INTERCONNECT_NAME_MAX + 1];
    InterconnectRole role;
    uint16_t port;
    uint16_t checkAliveSec;
    // Main peer when role is Repeater
    char mainName[INTERCONNECT_NAME_MAX + 1];
    uint8_t mainMac[6];
    char mainIp[16];
    uint16_t mainPort;
    uint8_t mainSecret[INTERCONNECT_SECRET_LEN];
    bool hasMain;
    bool mainOnline;
    uint32_t mainLastAliveMs;
};

class InterconnectStore {
public:
    InterconnectStore();
    ~InterconnectStore();

    InterconnectStore(const InterconnectStore &) = delete;
    InterconnectStore &operator=(const InterconnectStore &) = delete;

    bool begin(bool persistToLittleFs);
    void requestPersist();
    void persistIfDue();

    const InterconnectSettings &settings() const { return settingsState; }
    InterconnectSettings &settingsMutable() { return settingsState; }

    int peerCount() const;
    InterconnectPeer *firstPeer();
    const InterconnectPeer *firstPeer() const;
    InterconnectPeer *peerAt(int index);
    const InterconnectPeer *peerAt(int index) const;
    InterconnectPeer *findByMac(const uint8_t mac[6]);
    InterconnectPeer *findByMacText(const char *macText);
    InterconnectPeer *findByRouterId(uint32_t routerId);
    InterconnectPeer *findByIp(const char *ip);
    static bool parseMacText(const char *text, uint8_t mac[6]);

    bool setLocalIdentity(const char *name, InterconnectRole role, uint16_t port, uint16_t checkAliveSec);
    void clearPeers();
    bool setMainPeer(
        const char *name,
        const uint8_t mac[6],
        const char *ip,
        uint16_t port,
        const uint8_t secret[INTERCONNECT_SECRET_LEN]
    );
    InterconnectPeer *addPeer(
        const char *name,
        const uint8_t mac[6],
        const char *ip,
        uint16_t port,
        uint32_t routerId,
        uint8_t zigbeeChannel,
        const uint8_t secret[INTERCONNECT_SECRET_LEN]
    );
    bool removePeerByMac(const uint8_t mac[6]);
    /** Keep routerId + zigbeeChannel; replace MAC/IP/name/secret (hardware swap). */
    bool replacePeerEndpoint(
        uint32_t routerId,
        const char *name,
        const uint8_t mac[6],
        const char *ip,
        uint16_t port,
        const uint8_t secret[INTERCONNECT_SECRET_LEN]
    );

    uint32_t allocateRouterId() const;
    uint8_t allocateZigbeeChannel(uint8_t mainChannel) const;

    /** Recount bound Zigbee devices per paired Repeater router id. */
    void refreshDeviceCounts(DeviceTopicMap *topicMap);

    /** Operator/status JSON without secrets. */
    PsramJsonBuffer statusJson(uint32_t pairingWaitRemainingMs) const;
    PsramJsonBuffer exportJson() const;
    bool importJson(const char *json, bool keepExistingMacAuthoritative);
    bool loadFromFile(const char *path = INTERCONNECT_STORE_PATH);
    bool saveToFile(const char *path = INTERCONNECT_STORE_PATH);

private:
    InterconnectSettings settingsState{};
    InterconnectPeer peers[INTERCONNECT_MAX_REPEATERS]{};
    bool persistEnabled = false;
    bool persistPending = false;
    uint32_t persistDueMs = 0;

    void freePeerSlot(InterconnectPeer *peer);
    int usedPeerSlots() const;
};
