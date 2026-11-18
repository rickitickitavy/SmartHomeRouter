#pragma once

#if defined(BOARD_ROLE_HOST)

#include "InterconnectStore.h"
#include "PsramJsonBuffer.h"
#include "SpiProtocol.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <stdint.h>

enum InterconnectMsgType : uint8_t {
    InterconnectMsgReserved = 0,
    InterconnectMsgJustBooted = 1,
    InterconnectMsgVariable = 2,
    InterconnectMsgCheckAlive = 3,
    InterconnectMsgZigbeePacket = 4,
    InterconnectMsgSearchStart = 5,
    InterconnectMsgSearchStop = 6,
    InterconnectMsgRebind = 7,
    InterconnectMsgRebindResult = 8
};

enum InterconnectJobKind : uint8_t {
    InterconnectJobExchange = 1,
    InterconnectJobPairConnect = 2,
    InterconnectJobScanProbe = 3,
    InterconnectJobVersionProbe = 4
};

struct InterconnectJob {
    bool used;
    InterconnectJobKind kind;
    InterconnectMsgType type;
    uint8_t peerMac[6];
    char peerIp[16];
    uint16_t peerPort;
    uint16_t payloadLen;
    uint8_t payload[INTERCONNECT_AEAD_MAX_PAYLOAD];
};

class RouterInterconnect {
public:
    using ZigbeeInjectFn = void (*)(const SpiFrame &frame, uint32_t routerId);
    using SettingsApplyFn = void (*)(const char *name, const char *value);
    using LocalZigbeeChannelFn = uint8_t (*)();
    using LocalPermitJoinSecFn = uint8_t (*)();
    using ApplyZigbeeChannelFn = void (*)(uint8_t channel);
    using LiveChangedFn = void (*)();

    explicit RouterInterconnect(InterconnectStore *store);

    void begin();
    void loop();

    InterconnectStore *store() { return interconnectStore; }
    PsramJsonBuffer statusJson() const;

    bool enqueueExchange(
        InterconnectMsgType type,
        const uint8_t peerMac[6],
        const char *peerIp,
        uint16_t peerPort,
        const uint8_t *payload,
        uint16_t payloadLen
    );
    bool startPairingWait();
    void cancelPairingWait();
    bool pairingWaitActive() const { return pairingWaitActiveFlag; }
    uint32_t pairingWaitRemainingMs() const;

    bool startPairTo(const char *ip, uint16_t port);
    /** Same-MAC re-pair: try stored IP, else scan subnet for that MAC. */
    bool startRePair(const uint8_t expectedMac[6]);
    /** Pair (IP or empty=scan) and replace peer at keepRouterId (new hardware, same binds). */
    bool startPairReplace(const char *ip, uint16_t port, uint32_t keepRouterId);
    bool startSubnetScan(uint16_t port);
    void cancelSubnetScan();
    bool acceptScanCandidate();
    void rejectScanCandidate();
    bool scanActive() const { return scanActiveFlag; }
    bool scanHasCandidate() const { return scanCandidatePending; }
    const char *scanCandidateIp() const { return scanCandidate.ip; }
    const char *scanCandidateName() const { return scanCandidate.name; }
    const uint8_t *scanCandidateMac() const { return scanCandidate.mac; }
    uint16_t scanPort() const { return scanPortValue; }
    uint8_t scanProgressPercent() const;
    const char *rePairStatus() const { return rePairStatusText; }
    /** Main: rebind IEEE on a paired Repeater; wait for rebind_result. */
    bool rebindRemote(
        uint32_t routerId,
        const uint8_t ieee[8],
        uint8_t *outcomeOut,
        uint32_t timeoutMs = INTERCONNECT_REBIND_WAIT_MS
    );
    /** Repeater: forward a local SPI Zigbee event to Main when non-duplicate. */
    bool maybeForwardLocalSpiEvent(const SpiFrame &frame);
    void setZigbeeInjectHandler(ZigbeeInjectFn handler) { zigbeeInjectHandler = handler; }
    void setVariableApplyHandler(SettingsApplyFn handler) { variableApplyHandler = handler; }
    void setLocalZigbeeChannelProvider(LocalZigbeeChannelFn handler) {
        localZigbeeChannelProvider = handler;
    }
    void setLocalPermitJoinSecProvider(LocalPermitJoinSecFn handler) {
        localPermitJoinSecProvider = handler;
    }
    void setApplyZigbeeChannelHandler(ApplyZigbeeChannelFn handler) {
        applyZigbeeChannelHandler = handler;
    }
    void setLiveChangedHandler(LiveChangedFn handler) { liveChangedHandler = handler; }
    /** Main: push checkAlive, Zigbee search window, and peer channel to every paired Repeater. */
    void pushSharedVariablesToAllPeers();
    int queueUsed() const { return jobCount; }
    int queueDepth() const { return INTERCONNECT_QUEUE_DEPTH; }

    bool aeadSeal(
        const uint8_t secret[INTERCONNECT_SECRET_LEN],
        const uint8_t peerMac[6],
        uint8_t type,
        const uint8_t *payload,
        uint16_t payloadLen,
        uint8_t *outFrame,
        size_t outMax,
        size_t *outLen
    ) const;
    bool aeadOpen(
        const uint8_t secret[INTERCONNECT_SECRET_LEN],
        const uint8_t peerMac[6],
        const uint8_t *frame,
        size_t frameLen,
        uint8_t *typeOut,
        uint8_t *payloadOut,
        uint16_t payloadMax,
        uint16_t *payloadLenOut
    ) const;

    static bool localWifiMac(uint8_t macOut[6]);

private:
    InterconnectStore *interconnectStore = nullptr;
    WiFiServer *server = nullptr;
    InterconnectJob *jobQueue = nullptr;
    int jobCount = 0;
    SemaphoreHandle_t queueMutex = nullptr;
    TaskHandle_t workerTask = nullptr;
    bool pairingWaitActiveFlag = false;
    uint32_t pairingWaitDeadlineMs = 0;
    uint32_t lastCheckAliveMs = 0;
    ZigbeeInjectFn zigbeeInjectHandler = nullptr;
    SettingsApplyFn variableApplyHandler = nullptr;
    LocalZigbeeChannelFn localZigbeeChannelProvider = nullptr;
    LocalPermitJoinSecFn localPermitJoinSecProvider = nullptr;
    ApplyZigbeeChannelFn applyZigbeeChannelHandler = nullptr;
    LiveChangedFn liveChangedHandler = nullptr;
    mutable uint64_t nonceCounter = 1;

    void noteLiveUiChanged();

    bool enqueueJob(const InterconnectJob &job);
    void pumpQueue();
    void ensureWorkerStarted();
    static void workerEntry(void *argument);
    void workerLoop();
    void acceptClients();
    void handleAcceptedClient(WiFiClient &client);
    bool runUniversalExchangeAsClient(
        const char *ip,
        uint16_t port,
        const uint8_t secret[INTERCONNECT_SECRET_LEN],
        const uint8_t peerMac[6],
        InterconnectMsgType type,
        const uint8_t *payload,
        uint16_t payloadLen
    );
    bool runPairingAsInitiator(
        const char *ip,
        uint16_t port,
        const uint8_t *expectedMacOrNull,
        uint32_t replaceRouterIdOrZero
    );
    bool discoverIdentityAsInitiator(
        const char *ip,
        uint16_t port,
        char *nameOut,
        size_t nameMax,
        uint8_t macOut[6],
        char *versionOut,
        size_t versionMax
    );
    bool startSubnetScanForMac(const uint8_t expectedMac[6], uint16_t port);
    void clearRePairState();
    void setRePairStatus(const char *status);
    bool handlePairingAsListener(WiFiClient &client);
    bool readExact(WiFiClient &client, uint8_t *buf, size_t len, uint32_t timeoutMs);
    bool writeAll(WiFiClient &client, const uint8_t *buf, size_t len);
    bool handshakeAsInitiator(WiFiClient &client);
    bool handshakeAsListener(WiFiClient &client);
    void handleExchangePayload(uint8_t type, const uint8_t *payload, uint16_t len, const uint8_t peerMac[6]);
    void maybeSendCheckAlive();
    void maybeSendJustBooted();
    void maybeRefreshMainLiveness();
    void pushVariablesToPeer(const uint8_t peerMac[6], const char *peerIp, uint16_t peerPort);
    void onJustBootedFromPeer(const uint8_t peerMac[6], const uint8_t *payload, uint16_t len);
    void handleRebindRequest(const uint8_t *payload, uint16_t len);
    void handleRebindResult(const uint8_t *payload, uint16_t len);
    void pumpSubnetScan();
    void runScanProbeJob(const InterconnectJob &job);
    void runVersionProbeJob(const InterconnectJob &job);
    void maybePushSharedOnMainBoot();
    void requestVersionsFromAllPeers();
    bool enqueueVersionProbe(const char *ip, uint16_t port, const uint8_t expectedMac[6]);
    bool mainAllowsOutboundType(InterconnectMsgType type) const;
    bool peerAllowsOutboundType(const InterconnectPeer *peer, InterconnectMsgType type) const;
    bool enqueueScanProbe(const char *ip, uint16_t port);
    bool writeLocalIdentityReply(WiFiClient &client);
    bool readPeerIdentityReply(
        WiFiClient &client,
        char *nameOut,
        size_t nameMax,
        uint8_t macOut[6],
        char *versionOut,
        size_t versionMax
    );
    bool justBootedSent = false;
    bool mainBootPushDone = false;

    bool scanActiveFlag = false;
    bool scanCandidatePending = false;
    bool scanProbeInFlight = false;
    uint16_t scanPortValue = INTERCONNECT_PORT_DEFAULT;
    uint8_t scanBase[4]{};
    uint8_t scanHost = 1;
    uint8_t scanSelfHost = 0;
    uint8_t scanTried = 0;
    bool scanMatchMacOnly = false;
    bool scanAutoPairOnMatch = false;
    uint8_t scanExpectedMac[6]{};
    uint32_t scanReplaceRouterId = 0;
    struct ScanCandidate {
        char ip[16];
        char name[INTERCONNECT_NAME_MAX + 1];
        char hostVersion[INTERCONNECT_FW_VERSION_MAX + 1];
        uint8_t mac[6];
    };
    ScanCandidate scanCandidate{};

    enum RePairPhase : uint8_t {
        RePairPhaseIdle = 0,
        RePairPhaseTryIp = 1,
        RePairPhaseScanMac = 2
    };
    RePairPhase rePairPhase = RePairPhaseIdle;
    uint8_t rePairExpectedMac[6]{};
    uint32_t rePairKeepRouterId = 0;
    uint8_t rePairKeepChannel = 0;
    char rePairStatusText[24]{};

    bool rebindWaitActive = false;
    bool rebindWaitDone = false;
    uint8_t rebindWaitIeee[8]{};
    uint8_t rebindWaitOutcome = 0;
    uint32_t rebindWaitRouterId = 0;

};

#endif
