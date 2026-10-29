#pragma once

#include <Arduino.h>
#include "SpiProtocol.h"
#include "DeviceTopicMap.h"
#include "UserDataPayload.h"
#include "UserStore.h"

class ZigbeeSpiProxy {
public:
    using LightStateFn = void (*)(const char *message, const uint8_t ieee[8], uint8_t endpoint, uint16_t shortAddr);

    ZigbeeSpiProxy();
    void begin();
    void onSpiEvent(const SpiFrame &frame);
    bool permitJoin(uint8_t seconds);
    bool closeJoin();
    bool controlOnOff(const uint8_t ieee[8], const char *command, uint8_t endpoint);
    bool writeAttribute(
        const uint8_t ieee[8],
        uint8_t endpoint,
        uint16_t clusterId,
        uint16_t attributeId,
        uint8_t dataType,
        uint32_t attributeValue
    );
    bool readAttribute(
        const uint8_t ieee[8],
        uint8_t endpoint,
        uint16_t clusterId,
        uint16_t attributeId
    );
    bool sendClusterCommand(
        const uint8_t ieee[8],
        uint8_t endpoint,
        uint16_t clusterId,
        uint8_t commandId,
        const uint8_t *payload,
        uint8_t payloadLength
    );
    void queueDeletedIeeesAndPushAll(const uint8_t (*deletedIeees)[8], int deletedCount);
    void requestRegistryPull(DeviceTopicMap *topicMap);
    void requestUsersPull(UserStore *store);
    void publishHostStores(DeviceTopicMap *topicMap, UserStore *store);
    void requestDevicesFile();
    String devicesFileJson() const;
    void pumpRegistrySync();
    bool enqueueDeviceUpsert(const DeviceTopicEntry *entry);
    void forgetDeviceTelemetry(const uint8_t ieee[8]);
    bool enqueueDeviceDelete(const uint8_t ieee[8]);
    bool enqueueUserUpsert(const UserRecord *user);
    bool enqueueUserDelete(const char *userName);
    void queueDeletedUserNamesAndPushAll(const char (*removedNames)[USER_NAME_MAX], int removedCount);
    bool registryHydrated() const;
    bool isOnline(const uint8_t ieee[8]) const;
    bool lastRssiDbm(const uint8_t ieee[8], int8_t *rssiDbm) const;
    void appendListTelemetry(const uint8_t ieee[8], String &json) const;
    void noteMqttState(const uint8_t ieee[8], uint8_t endpoint, const char *message);
    void retainRegisteredTelemetry(DeviceTopicMap *topicMap);
    void noteSeen(const uint8_t ieee[8]);
    void noteMqttOffline(const uint8_t ieee[8]);
    void notePacketReceived();
    void notePacketSent();
    uint32_t packetsReceived() const;
    uint32_t packetsSent() const;
    bool pairingActive() const;
    int onlineCount() const;
    String devicesJson(DeviceTopicMap *topicMap);
    void setLightStateHandler(LightStateFn handler);
    void setRegistryPullDoneHandler(void (*handler)());
    using DeviceLiveChangedFn = void (*)(const uint8_t ieee[8]);
    void setDeviceLiveChangedHandler(DeviceLiveChangedFn handler);
    using StatusValueChangedFn =
        void (*)(
            const uint8_t ieee[8],
            uint8_t endpoint,
            uint16_t clusterOr0,
            const char *newState,
            bool valueChanged
        );
    void setStatusValueChangedHandler(StatusValueChangedFn handler);
    bool copyEndpointState(const uint8_t ieee[8], uint8_t endpoint, char *out, size_t outLen) const;
    bool copyEndpointStateForParam(
        const uint8_t ieee[8],
        uint8_t endpoint,
        const char *param,
        char *out,
        size_t outLen
    ) const;
    void pumpOnlineWindows();
    bool commandsAllowed() const;

    bool beginConfigReads(const uint8_t ieee[8], const uint16_t *clusterIds, const uint16_t *attributeIds, uint8_t count);
    bool noteConfigAttrReply(
        const uint8_t ieee[8],
        uint16_t clusterId,
        uint16_t attributeId,
        uint32_t value
    );
    bool configReadsComplete(unsigned long startedMs, unsigned long timeoutMs) const;
    bool configReadValue(
        const uint8_t ieee[8],
        uint16_t clusterId,
        uint16_t attributeId,
        uint32_t *valueOut,
        bool *haveValueOut
    ) const;
    void clearConfigReads();

private:
    static constexpr uint8_t kConfigReadSlots = 8;
    static constexpr int kPendingChangeSlots = 1;
    static constexpr unsigned long kOnlineWindowMs = 15UL * 60UL * 1000UL;

    // Host cache only (MQTT / UI / automations). SPI wire messages stay SPI_DEVICE_MESSAGE_MAX.
    // Status text is heap-sized up to userData brace values: 24 slots × 16 bytes (15 chars + NUL).
    static constexpr size_t kStatusStateMax = kUserDataValuesMaxBytes;

    struct CoveringStatus {
        char move[16];
        char position[8];
        char discrete[16];
    };

    struct EndpointStatus {
        uint8_t endpoint;
        char *state;
        CoveringStatus *covering;
    };
    static_assert(sizeof(EndpointStatus) < 64, "EndpointStatus must not embed status buffers");

    struct ReportableValueSlot {
        uint8_t endpoint;
        uint16_t cluster;
        uint16_t attribute;
        char *value;
    };
    static constexpr uint8_t kReportableValueSlotMax = 32;

    struct CachedDevice {
        CachedDevice *next;
        uint8_t ieee[8];
        uint16_t shortAddr;
        uint8_t endpoint;
        uint8_t channelCapacity;
        EndpointStatus *channels;
        char *manufacturer;
        char *model;
        unsigned long lastSeenMs;
        bool consoleOnlineSent;
        int8_t lastRssiDbm;
        bool hasRssi;
        bool hasBattery;
        unsigned batteryPercent;
        ReportableValueSlot *reportableValues;
        uint8_t reportableValueCount;
    };
    static_assert(sizeof(CachedDevice) < 112, "CachedDevice must not embed manufacturer/model arrays");

    struct PendingDeviceChange {
        bool used;
        uint8_t flags;
        DeviceTopicEntry *entry;
    };

    struct PendingUserChange {
        bool used;
        uint8_t flags;
        UserRecord user;
    };

    CachedDevice *deviceHead = nullptr;
    LightStateFn lightStateHandler = nullptr;
    DeviceLiveChangedFn deviceLiveChanged = nullptr;
    StatusValueChangedFn statusValueChanged = nullptr;
    DeviceTopicMap *registryMap = nullptr;
    DeviceTopicMap pullMap;
    PendingDeviceChange pendingChanges[kPendingChangeSlots]{};
    UserStore *userMap = nullptr;
    UserStore pullUsers;
    PendingUserChange pendingUserChanges[kPendingChangeSlots]{};
    bool userPullRequested = false;
    bool userPullActive = false;
    bool userReady = false;
    bool userCollecting = false;
    int userPullExpectedCount = -1;
    uint8_t userPullRetries = 0;
    unsigned long userPullStartedMs = 0;
    char pendingDeleteUserNames[USER_STORE_MAX][USER_NAME_MAX]{};
    int pendingUserDeleteCount = 0;
    int pendingUserDeleteIndex = 0;
    int pendingUserUpsertWalk = 0;
    bool userFullPushActive = false;
    bool registryPullRequested = false;
    bool registryPullActive = false;
    bool registryReady = false;
    bool pullCollecting = false;
    int registryPullCount = 0;
    int pullExpectedCount = -1;
    uint8_t pullRetries = 0;
    unsigned long registryPullStartedMs = 0;
    void (*registryPullDone)() = nullptr;
    bool filePullRequested = false;
    bool filePullCollecting = false;
    bool filePullInFlight = false;
    unsigned long filePullStartedMs = 0;
    String filePullBuffer;
    String fileCache;
    uint32_t packetsRx = 0;
    uint32_t packetsTx = 0;
    bool pairingOpen = false;
    struct PendingDeleteIeeeNode {
        uint8_t ieee[8];
        PendingDeleteIeeeNode *next;
    };
    PendingDeleteIeeeNode *pendingDeleteHead = nullptr;
    int pendingDeleteCount = 0;
    int pendingDeleteIndex = 0;
    int pendingUpsertWalk = 0;
    bool fullPushActive = false;

    struct ConfigReadSlot {
        bool used;
        bool haveValue;
        uint8_t ieee[8];
        uint16_t clusterId;
        uint16_t attributeId;
        uint32_t value;
    };
    ConfigReadSlot configReads[kConfigReadSlots]{};

    CachedDevice *findByIeee(const uint8_t ieee[8]);
    const CachedDevice *findByIeee(const uint8_t ieee[8]) const;
    uint8_t channelCapacityForIeee(const uint8_t ieee[8]) const;
    bool ensureChannelCapacity(CachedDevice *slot, uint8_t neededCapacity);
    CachedDevice *allocSlot(const uint8_t ieee[8]);
    void releaseSlot(CachedDevice *slot);
    static bool endpointStatusOccupied(const EndpointStatus *status);
    static void clearEndpointStatus(EndpointStatus *status);
    static bool setEndpointState(EndpointStatus *status, const char *text);
    static CoveringStatus *ensureCoveringStatus(EndpointStatus *status);
    static void assignBoundedField(char *&destination, const char *source, size_t maxLen);
    void noteReportTelemetry(CachedDevice *slot, uint8_t endpoint, const char *message);
    void noteReportableValue(
        CachedDevice *slot,
        uint8_t endpoint,
        uint16_t cluster,
        uint16_t attribute,
        const char *valueText
    );
    void clearReportableValues(CachedDevice *slot);
    void notifyDeviceLiveChanged(const uint8_t ieee[8]);
    bool enqueueRegistryFrame(uint8_t flags, const DeviceTopicEntry *entry);
    bool queueDeviceChange(uint8_t flags, const DeviceTopicEntry *entry);
    void applyPendingChangeToMap(const PendingDeviceChange *change);
    void replayPendingChanges();
    void pumpPendingChanges();
    void beginPullSnapshot();
    void applyPulledRegistry(const SpiFrame &frame);
    void finishRegistryPull();
    void clearPendingDeleteIeees();
    void applyDevicesFile(const SpiFrame &frame);
    bool enqueueUserFrame(uint8_t flags, const UserRecord *user);
    bool queueUserChange(uint8_t flags, const UserRecord *user);
    void applyPendingUserChangeToStore(const PendingUserChange *change);
    void replayPendingUserChanges();
    void pumpPendingUserChanges();
    void beginUserPullSnapshot();
    void applyPulledUsers(const SpiFrame &frame);
    void finishUsersPull();
};

extern ZigbeeSpiProxy ZIGBEE_SPI_PROXY;
