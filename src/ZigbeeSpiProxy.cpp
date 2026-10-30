#include "ZigbeeSpiProxy.h"
#include "BoundDeviceJson.h"
#include "InterChipHost.h"
#include "JsonField.h"
#include "Logger.h"
#include "UserDataPayload.h"
#include "ZigbeeCluster.h"
#include "ZigbeeDeviceType.h"

#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ZigbeeSpiProxy ZIGBEE_SPI_PROXY;

ZigbeeSpiProxy::ZigbeeSpiProxy() {}

void ZigbeeSpiProxy::begin() {}

void ZigbeeSpiProxy::setLightStateHandler(LightStateFn handler) {
    lightStateHandler = handler;
}

bool ZigbeeSpiProxy::commandsAllowed() const {
    return INTER_CHIP_HOST.isNormal();
}

ZigbeeSpiProxy::CachedDevice *ZigbeeSpiProxy::findByIeee(const uint8_t ieee[8]) {
    return const_cast<CachedDevice *>(static_cast<const ZigbeeSpiProxy *>(this)->findByIeee(ieee));
}

const ZigbeeSpiProxy::CachedDevice *ZigbeeSpiProxy::findByIeee(const uint8_t ieee[8]) const {
    if (ieee == nullptr) {
        return nullptr;
    }
    for (const CachedDevice *slot = deviceHead; slot != nullptr; slot = slot->next) {
        if (memcmp(slot->ieee, ieee, 8) == 0) {
            return slot;
        }
    }
    return nullptr;
}

uint8_t ZigbeeSpiProxy::channelCapacityForIeee(const uint8_t ieee[8]) const {
    if (ieee == nullptr) {
        return 1;
    }
    const DeviceTopicEntry *entry = registryMap != nullptr ? registryMap->findByIeee(ieee) : nullptr;
    if (entry == nullptr || !entry->used) {
        return 1;
    }
    if (entry->channelCount == DEVICE_CHANNEL_PARSE) {
        return DEVICE_CHANNEL_COUNT_MAX;
    }
    if (entry->channelCount >= 2) {
        return entry->channelCount;
    }
    return 1;
}

bool ZigbeeSpiProxy::endpointStatusOccupied(const EndpointStatus *status) {
    if (status == nullptr) {
        return false;
    }
    if (status->state != nullptr && status->state[0] != '\0') {
        return true;
    }
    if (status->covering == nullptr) {
        return false;
    }
    return status->covering->move[0] != '\0'
        || status->covering->position[0] != '\0'
        || status->covering->discrete[0] != '\0';
}

void ZigbeeSpiProxy::clearEndpointStatus(EndpointStatus *status) {
    if (status == nullptr) {
        return;
    }
    free(status->state);
    status->state = nullptr;
    free(status->covering);
    status->covering = nullptr;
    status->endpoint = 0;
}

bool ZigbeeSpiProxy::setEndpointState(EndpointStatus *status, const char *text) {
    if (status == nullptr) {
        return false;
    }
    if (text == nullptr || text[0] == '\0') {
        free(status->state);
        status->state = nullptr;
        return true;
    }
    size_t copyLen = strlen(text);
    if (copyLen >= kStatusStateMax) {
        copyLen = kStatusStateMax - 1;
    }
    if (status->state != nullptr
        && strncmp(status->state, text, copyLen) == 0
        && status->state[copyLen] == '\0') {
        return true;
    }
    char *copy = (char *)malloc(copyLen + 1);
    if (copy == nullptr) {
        LOGGER.warning("Host status string alloc failed");
        return false;
    }
    memcpy(copy, text, copyLen);
    copy[copyLen] = '\0';
    free(status->state);
    status->state = copy;
    return true;
}

ZigbeeSpiProxy::CoveringStatus *ZigbeeSpiProxy::ensureCoveringStatus(EndpointStatus *status) {
    if (status == nullptr) {
        return nullptr;
    }
    if (status->covering != nullptr) {
        return status->covering;
    }
    CoveringStatus *covering = (CoveringStatus *)calloc(1, sizeof(CoveringStatus));
    if (covering == nullptr) {
        LOGGER.warning("Host covering status alloc failed");
        return nullptr;
    }
    status->covering = covering;
    return covering;
}

void ZigbeeSpiProxy::assignBoundedField(char *&destination, const char *source, size_t maxLen) {
    free(destination);
    destination = nullptr;
    if (source == nullptr || source[0] == '\0' || maxLen == 0) {
        return;
    }
    destination = DeviceTopicMap::duplicateBoundedString(source, maxLen);
}

bool ZigbeeSpiProxy::ensureChannelCapacity(CachedDevice *slot, uint8_t neededCapacity) {
    if (slot == nullptr || neededCapacity == 0) {
        return false;
    }
    if (neededCapacity > DEVICE_CHANNEL_COUNT_MAX) {
        neededCapacity = DEVICE_CHANNEL_COUNT_MAX;
    }
    if (slot->channels != nullptr && slot->channelCapacity >= neededCapacity) {
        return true;
    }
    EndpointStatus *grown = new (std::nothrow) EndpointStatus[neededCapacity];
    if (grown == nullptr) {
        LOGGER.warning("Host channel cache alloc failed");
        return slot->channels != nullptr;
    }
    memset(grown, 0, sizeof(EndpointStatus) * neededCapacity);
    if (slot->channels != nullptr) {
        const uint8_t copyCount =
            slot->channelCapacity < neededCapacity ? slot->channelCapacity : neededCapacity;
        memcpy(grown, slot->channels, sizeof(EndpointStatus) * copyCount);
        // Ownership of state/covering pointers moved into grown; do not clearEndpointStatus.
        delete[] slot->channels;
    }
    slot->channels = grown;
    slot->channelCapacity = neededCapacity;
    return true;
}

void ZigbeeSpiProxy::releaseSlot(CachedDevice *slot) {
    if (slot == nullptr) {
        return;
    }
    if (slot->channels != nullptr) {
        for (uint8_t index = 0; index < slot->channelCapacity; index++) {
            clearEndpointStatus(&slot->channels[index]);
        }
        delete[] slot->channels;
    }
    slot->channels = nullptr;
    slot->channelCapacity = 0;
    free(slot->manufacturer);
    slot->manufacturer = nullptr;
    free(slot->model);
    slot->model = nullptr;
    clearReportableValues(slot);
    delete slot;
}

void ZigbeeSpiProxy::clearReportableValues(CachedDevice *slot) {
    if (slot == nullptr) {
        return;
    }
    if (slot->reportableValues != nullptr) {
        for (uint8_t index = 0; index < slot->reportableValueCount; index++) {
            free(slot->reportableValues[index].value);
            slot->reportableValues[index].value = nullptr;
        }
        free(slot->reportableValues);
        slot->reportableValues = nullptr;
    }
    slot->reportableValueCount = 0;
}

void ZigbeeSpiProxy::noteReportableValue(
    CachedDevice *slot,
    uint8_t endpoint,
    uint16_t cluster,
    uint16_t attribute,
    const char *valueText
) {
    if (slot == nullptr || valueText == nullptr || !DeviceTopicMap::isUsableEndpoint(endpoint)) {
        return;
    }
    if (slot->reportableValues != nullptr) {
        for (uint8_t index = 0; index < slot->reportableValueCount; index++) {
            ReportableValueSlot &existing = slot->reportableValues[index];
            if (existing.endpoint == endpoint
                && existing.cluster == cluster
                && existing.attribute == attribute) {
                free(existing.value);
                existing.value = DeviceTopicMap::duplicateBoundedString(
                    valueText,
                    kReportableValueTextMaxLen
                );
                return;
            }
        }
    }
    if (slot->reportableValueCount >= kReportableValueSlotMax) {
        return;
    }
    ReportableValueSlot *grown = (ReportableValueSlot *)realloc(
        slot->reportableValues,
        (size_t)(slot->reportableValueCount + 1) * sizeof(ReportableValueSlot)
    );
    if (grown == nullptr) {
        return;
    }
    slot->reportableValues = grown;
    ReportableValueSlot &created = slot->reportableValues[slot->reportableValueCount];
    created.endpoint = endpoint;
    created.cluster = cluster;
    created.attribute = attribute;
    created.value = DeviceTopicMap::duplicateBoundedString(valueText, kReportableValueTextMaxLen);
    slot->reportableValueCount++;
}

void ZigbeeSpiProxy::noteReportTelemetry(CachedDevice *slot, uint8_t endpoint, const char *message) {
    if (slot == nullptr || message == nullptr || message[0] == '\0') {
        return;
    }
    unsigned parsedPercent = 0;
    if (sscanf(message, "BATTERY %u", &parsedPercent) == 1) {
        const bool batteryChanged =
            !slot->hasBattery || slot->batteryPercent != (uint8_t)parsedPercent;
        slot->hasBattery = true;
        slot->batteryPercent = parsedPercent;
        char batteryMessage[16];
        snprintf(batteryMessage, sizeof(batteryMessage), "%u", parsedPercent);
        noteReportableValue(
            slot,
            endpoint,
            kZigbeeClusterPowerConfig,
            kZigbeeAttrBatteryPercentageRemaining,
            batteryMessage
        );
        // Battery may re-report the same %; pass valueChanged so listeners can filter by eventType.
        // Notify with a plain number so GREATER/LESS compares work (no "BATTERY " prefix).
        if (statusValueChanged != nullptr && DeviceTopicMap::isUsableEndpoint(endpoint)) {
            statusValueChanged(
                slot->ieee,
                endpoint,
                kZigbeeClusterPowerConfig,
                batteryMessage,
                batteryChanged
            );
        }
        return;
    }
    unsigned parsedCluster = 0;
    unsigned parsedAttr = 0;
    unsigned long parsedValue = 0;
    char reportableConverted[kReportableValueTextMaxLen];
    reportableConverted[0] = '\0';
    const bool haveClAttrVal =
        sscanf(message, "cl=0x%X,attr=0x%X,val=0x%lX", &parsedCluster, &parsedAttr, &parsedValue) == 3
        || sscanf(
               message,
               "%*[^ ] cl=0x%X,attr=0x%X,val=0x%lX",
               &parsedCluster,
               &parsedAttr,
               &parsedValue
           ) == 3
        || sscanf(message, "cl=0x%X,attr=0x%X,val=%lu", &parsedCluster, &parsedAttr, &parsedValue) == 3
        || sscanf(
               message,
               "%*[^ ] cl=0x%X,attr=0x%X,val=%lu",
               &parsedCluster,
               &parsedAttr,
               &parsedValue
           ) == 3;
    if (haveClAttrVal && registryMap != nullptr) {
        const DeviceTopicEntry *entry = registryMap->findByIeee(slot->ieee);
        if (entry != nullptr && entry->reportableDataCount > 0) {
            const int rowIndex = reportableDataFindIndex(
                entry->reportableData,
                entry->reportableDataCount,
                (uint16_t)parsedCluster,
                (uint16_t)parsedAttr
            );
            if (rowIndex >= 0) {
                if (reportableDataConvertZclValue(
                        (uint16_t)parsedCluster,
                        (uint16_t)parsedAttr,
                        (uint32_t)parsedValue,
                        (ReportableDataType)entry->reportableData[rowIndex].type,
                        reportableConverted,
                        sizeof(reportableConverted)
                    )) {
                    noteReportableValue(
                        slot,
                        endpoint,
                        (uint16_t)parsedCluster,
                        (uint16_t)parsedAttr,
                        reportableConverted
                    );
                    if ((uint16_t)parsedCluster == kZigbeeClusterPowerConfig
                        && (uint16_t)parsedAttr == kZigbeeAttrBatteryPercentageRemaining) {
                        unsigned batteryFromConverted = 0;
                        if (sscanf(reportableConverted, "%u", &batteryFromConverted) == 1) {
                            slot->hasBattery = true;
                            slot->batteryPercent = batteryFromConverted;
                        }
                    }
                    // Prefer converted text for endpoint status so legacy gauges keep working.
                    message = reportableConverted;
                }
            } else if ((uint16_t)parsedCluster != kUserDataClusterId
                       && (uint16_t)parsedCluster != kZigbeeClusterOnOff
                       && (uint16_t)parsedCluster != kZigbeeClusterIasZone
                       && (uint16_t)parsedCluster != kZigbeeClusterWindowCovering) {
                // Always log unknown reportable pairs, even when device DEBUG is off.
                char ieeeText[24];
                snprintf(
                    ieeeText,
                    sizeof(ieeeText),
                    "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                    slot->ieee[7],
                    slot->ieee[6],
                    slot->ieee[5],
                    slot->ieee[4],
                    slot->ieee[3],
                    slot->ieee[2],
                    slot->ieee[1],
                    slot->ieee[0]
                );
                LOGGER.info(
                    String("Unknown reportable cl=0x")
                    + String(parsedCluster, HEX)
                    + ",attr=0x"
                    + String(parsedAttr, HEX)
                    + " ieee="
                    + ieeeText
                );
            }
        }
    }
    String userDataBraceList;
    bool haveUserDataBrace = false;
    if (strstr(message, "cl=0x") != nullptr && strstr(message, "attr=") != nullptr) {
        // Dispatch by cluster in the packet — only 0xFC00 uses user-data brace schema.
        if (userDataMessageIsClusterReport(message)) {
            if (userDataExtractBraceList(message, &userDataBraceList) && userDataBraceList.length() > 0) {
                haveUserDataBrace = true;
            }
            // Non-brace 0xFC00 vals still fall through as shaped status text.
        }
        // Other clusters (On/Off, Power Config, …) fall through unchanged.
    } else {
        const char *braceCursor = message;
        while (*braceCursor == ' ' || *braceCursor == '\t') {
            braceCursor++;
        }
        if (*braceCursor == '{') {
            bool isUserDataDevice = false;
            if (registryMap != nullptr) {
                const DeviceTopicEntry *entry = registryMap->findByIeee(slot->ieee);
                isUserDataDevice =
                    entry != nullptr && zigbeeDeviceTypeIsUserData(entry->zigbeeType);
            }
            if (isUserDataDevice
                && userDataExtractBraceList(message, &userDataBraceList)
                && userDataBraceList.length() > 0) {
                haveUserDataBrace = true;
            }
        }
    }
    if (haveUserDataBrace) {
        userDataBraceList = userDataClampBraceListValues(userDataBraceList);
        message = userDataBraceList.c_str();
    }
    if (!DeviceTopicMap::isUsableEndpoint(endpoint)) {
        return;
    }
    uint8_t neededCapacity = channelCapacityForIeee(slot->ieee);
    if (endpoint > neededCapacity && neededCapacity < DEVICE_CHANNEL_COUNT_MAX) {
        neededCapacity = endpoint;
    }
    if (!ensureChannelCapacity(slot, neededCapacity)) {
        return;
    }
    EndpointStatus *statusSlot = nullptr;
    for (uint8_t i = 0; i < slot->channelCapacity; i++) {
        if (slot->channels[i].endpoint == endpoint && endpointStatusOccupied(&slot->channels[i])) {
            statusSlot = &slot->channels[i];
            break;
        }
    }
    if (statusSlot == nullptr) {
        for (uint8_t i = 0; i < slot->channelCapacity; i++) {
            if (!endpointStatusOccupied(&slot->channels[i])) {
                statusSlot = &slot->channels[i];
                statusSlot->endpoint = endpoint;
                break;
            }
        }
    }
    if (statusSlot == nullptr) {
        LOGGER.warning("Host channel cache full for device");
        return;
    }
    const bool valueChanged =
        statusSlot->state == nullptr || strcmp(statusSlot->state, message) != 0;
    if (!setEndpointState(statusSlot, message)) {
        return;
    }
    const bool isMove = strcmp(message, "UP") == 0
        || strcmp(message, "DOWN") == 0
        || strcmp(message, "STOPPED") == 0;
    const bool isOnOffOrZone = strcmp(message, "ON") == 0
        || strcmp(message, "OFF") == 0
        || strcmp(message, "LEAK") == 0
        || strcmp(message, "DRY") == 0;
    const bool isDiscreteStatus = isMove || isOnOffOrZone;
    bool isWindowCoveringDevice = false;
    if (registryMap != nullptr) {
        const DeviceTopicEntry *entry = registryMap->findByIeee(slot->ieee);
        if (entry != nullptr && entry->zigbeeType == ZigbeeDeviceTypeWindowCovering) {
            isWindowCoveringDevice = true;
        }
    }
    char *positionEnd = nullptr;
    const unsigned long positionParsed = strtoul(message, &positionEnd, 10);
    const bool legacyPositionPercent =
        positionEnd != message && *positionEnd == '%' && positionEnd[1] == '\0';
    const bool plainPosition =
        isWindowCoveringDevice
        && positionEnd != message
        && *positionEnd == '\0'
        && positionParsed <= 100UL;
    const bool isPosition = legacyPositionPercent || plainPosition;
    if (isMove) {
        CoveringStatus *covering = ensureCoveringStatus(statusSlot);
        if (covering != nullptr) {
            strncpy(covering->move, message, sizeof(covering->move) - 1);
            covering->move[sizeof(covering->move) - 1] = '\0';
        }
    } else if (isOnOffOrZone) {
        CoveringStatus *covering = ensureCoveringStatus(statusSlot);
        if (covering != nullptr) {
            strncpy(covering->discrete, message, sizeof(covering->discrete) - 1);
            covering->discrete[sizeof(covering->discrete) - 1] = '\0';
        }
    } else if (isPosition) {
        // Store plain integer only — strip legacy trailing '%' for GREATER/LESS compares.
        CoveringStatus *covering = ensureCoveringStatus(statusSlot);
        char positionText[8];
        snprintf(positionText, sizeof(positionText), "%lu", positionParsed);
        if (covering != nullptr) {
            strncpy(covering->position, positionText, sizeof(covering->position) - 1);
            covering->position[sizeof(covering->position) - 1] = '\0';
        }
        setEndpointState(statusSlot, positionText);
    } else {
        const char *space = strchr(message, ' ');
        if (space != nullptr) {
            char moveToken[16];
            const size_t moveLen = (size_t)(space - message);
            if (moveLen > 0 && moveLen < sizeof(moveToken)) {
                memcpy(moveToken, message, moveLen);
                moveToken[moveLen] = '\0';
                char *legacyPosEnd = nullptr;
                const unsigned long legacyPos = strtoul(space + 1, &legacyPosEnd, 10);
                const bool legacyMove = strcmp(moveToken, "UP") == 0
                    || strcmp(moveToken, "DOWN") == 0
                    || strcmp(moveToken, "STOPPED") == 0;
                const bool legacyPosOk =
                    legacyPosEnd != (space + 1)
                    && ((*legacyPosEnd == '%' && legacyPosEnd[1] == '\0')
                        || *legacyPosEnd == '\0');
                if (legacyMove && legacyPosOk) {
                    CoveringStatus *covering = ensureCoveringStatus(statusSlot);
                    if (covering != nullptr) {
                        strncpy(covering->move, moveToken, sizeof(covering->move) - 1);
                        covering->move[sizeof(covering->move) - 1] = '\0';
                        snprintf(
                            covering->position,
                            sizeof(covering->position),
                            "%lu",
                            legacyPos
                        );
                    }
                }
            }
        }
    }
    // Discrete statuses may re-report the same value; still notify so eventType=any can re-fire.
    // Listeners with eventType=change filter using valueChanged.
    if ((valueChanged || isDiscreteStatus) && statusValueChanged != nullptr) {
        uint16_t clusterOr0 = 0;
        const char *clusterMark = strstr(message, "cl=0x");
        if (clusterMark != nullptr) {
            clusterOr0 = (uint16_t)strtoul(clusterMark + 5, nullptr, 16);
        } else {
            clusterMark = strstr(message, "cl=");
            if (clusterMark != nullptr) {
                clusterOr0 = (uint16_t)strtoul(clusterMark + 3, nullptr, 0);
            }
        }
        if (clusterOr0 == 0) {
            if (isOnOffOrZone) {
                if (strcmp(message, "LEAK") == 0 || strcmp(message, "DRY") == 0) {
                    clusterOr0 = kZigbeeClusterIasZone;
                } else {
                    clusterOr0 = kZigbeeClusterOnOff;
                }
            } else if (isMove || isPosition) {
                clusterOr0 = kZigbeeClusterWindowCovering;
            }
        }
        const char *notifyState = statusSlot->state != nullptr ? statusSlot->state : "";
        statusValueChanged(slot->ieee, endpoint, clusterOr0, notifyState, valueChanged);
    }
}

ZigbeeSpiProxy::CachedDevice *ZigbeeSpiProxy::allocSlot(const uint8_t ieee[8]) {
    CachedDevice *existing = findByIeee(ieee);
    if (existing != nullptr) {
        ensureChannelCapacity(existing, channelCapacityForIeee(ieee));
        return existing;
    }
    CachedDevice *slot = new (std::nothrow) CachedDevice();
    if (slot == nullptr) {
        LOGGER.warning("Host device cache alloc failed");
        return nullptr;
    }
    memset(slot, 0, sizeof(CachedDevice));
    memcpy(slot->ieee, ieee, 8);
    slot->lastSeenMs = millis();
    if (!ensureChannelCapacity(slot, channelCapacityForIeee(ieee))) {
        releaseSlot(slot);
        return nullptr;
    }
    slot->next = deviceHead;
    deviceHead = slot;
    return slot;
}

void ZigbeeSpiProxy::retainRegisteredTelemetry(DeviceTopicMap *topicMap) {
    registryMap = topicMap;
    CachedDevice **link = &deviceHead;
    while (*link != nullptr) {
        CachedDevice *slot = *link;
        if (topicMap != nullptr && topicMap->findByIeee(slot->ieee) != nullptr) {
            ensureChannelCapacity(slot, channelCapacityForIeee(slot->ieee));
            link = &slot->next;
            continue;
        }
        *link = slot->next;
        releaseSlot(slot);
    }
}

bool ZigbeeSpiProxy::permitJoin(uint8_t seconds) {
    const bool queued = INTER_CHIP_HOST.tryEnqueue(SpiCmdPermitJoin, &seconds, 1);
    if (queued) {
        pairingOpen = seconds != 0;
    }
    return queued;
}

bool ZigbeeSpiProxy::closeJoin() {
    uint8_t seconds = 0;
    const bool queued = INTER_CHIP_HOST.tryEnqueue(SpiCmdPermitJoin, &seconds, 1);
    if (queued) {
        pairingOpen = false;
    }
    return queued;
}

bool ZigbeeSpiProxy::controlOnOff(const uint8_t ieee[8], const char *command, uint8_t endpoint) {
    const char *body = command != nullptr ? command : "";
    uint8_t payload[9 + SPI_DEVICE_MESSAGE_MAX];
    memcpy(payload, ieee, 8);
    payload[8] = endpoint;
    strncpy((char *)payload + 9, body, SPI_DEVICE_MESSAGE_MAX - 1);
    payload[8 + SPI_DEVICE_MESSAGE_MAX] = 0;
    const uint16_t length = (uint16_t)(9 + strlen((char *)payload + 9) + 1);
    const bool queued = INTER_CHIP_HOST.tryEnqueue(SpiCmdZclOnOff, payload, length);
    if (queued) {
        packetsTx++;
    }
    return queued;
}

bool ZigbeeSpiProxy::writeAttribute(
    const uint8_t ieee[8],
    uint8_t endpoint,
    uint16_t clusterId,
    uint16_t attributeId,
    uint8_t dataType,
    uint32_t attributeValue
) {
    uint8_t payload[SPI_ZCL_WRITE_ATTR_LEN];
    if (!spiPackZclWriteAttr(
            payload,
            sizeof(payload),
            ieee,
            endpoint,
            clusterId,
            attributeId,
            dataType,
            attributeValue
        )) {
        return false;
    }
    const bool queued = INTER_CHIP_HOST.tryEnqueue(SpiCmdZclWriteAttr, payload, SPI_ZCL_WRITE_ATTR_LEN);
    if (queued) {
        packetsTx++;
    }
    return queued;
}

bool ZigbeeSpiProxy::readAttribute(
    const uint8_t ieee[8],
    uint8_t endpoint,
    uint16_t clusterId,
    uint16_t attributeId
) {
    uint8_t payload[SPI_ZCL_READ_ATTR_LEN];
    if (!spiPackZclReadAttr(payload, sizeof(payload), ieee, endpoint, clusterId, attributeId)) {
        return false;
    }
    const bool queued = INTER_CHIP_HOST.tryEnqueue(SpiCmdZclReadAttr, payload, SPI_ZCL_READ_ATTR_LEN);
    if (queued) {
        packetsTx++;
    }
    return queued;
}

void ZigbeeSpiProxy::clearConfigReads() {
    memset(configReads, 0, sizeof(configReads));
}

bool ZigbeeSpiProxy::beginConfigReads(
    const uint8_t ieee[8],
    const uint16_t *clusterIds,
    const uint16_t *attributeIds,
    uint8_t count
) {
    clearConfigReads();
    if (ieee == nullptr || clusterIds == nullptr || attributeIds == nullptr || count == 0) {
        return false;
    }
    if (count > kConfigReadSlots) {
        count = kConfigReadSlots;
    }
    for (uint8_t index = 0; index < count; index++) {
        configReads[index].used = true;
        configReads[index].haveValue = false;
        memcpy(configReads[index].ieee, ieee, 8);
        configReads[index].clusterId = clusterIds[index];
        configReads[index].attributeId = attributeIds[index];
        configReads[index].value = 0;
    }
    return true;
}

bool ZigbeeSpiProxy::noteConfigAttrReply(
    const uint8_t ieee[8],
    uint16_t clusterId,
    uint16_t attributeId,
    uint32_t value
) {
    if (ieee == nullptr) {
        return false;
    }
    for (uint8_t index = 0; index < kConfigReadSlots; index++) {
        if (!configReads[index].used || configReads[index].haveValue) {
            continue;
        }
        if (memcmp(configReads[index].ieee, ieee, 8) != 0) {
            continue;
        }
        if (configReads[index].clusterId != clusterId || configReads[index].attributeId != attributeId) {
            continue;
        }
        configReads[index].value = value;
        configReads[index].haveValue = true;
        return true;
    }
    return false;
}

bool ZigbeeSpiProxy::configReadsComplete(unsigned long startedMs, unsigned long timeoutMs) const {
    bool anyPending = false;
    for (uint8_t index = 0; index < kConfigReadSlots; index++) {
        if (!configReads[index].used) {
            continue;
        }
        if (!configReads[index].haveValue) {
            anyPending = true;
            break;
        }
    }
    if (!anyPending) {
        return true;
    }
    return (millis() - startedMs) >= timeoutMs;
}

bool ZigbeeSpiProxy::configReadValue(
    const uint8_t ieee[8],
    uint16_t clusterId,
    uint16_t attributeId,
    uint32_t *valueOut,
    bool *haveValueOut
) const {
    if (haveValueOut != nullptr) {
        *haveValueOut = false;
    }
    if (ieee == nullptr) {
        return false;
    }
    for (uint8_t index = 0; index < kConfigReadSlots; index++) {
        if (!configReads[index].used) {
            continue;
        }
        if (memcmp(configReads[index].ieee, ieee, 8) != 0) {
            continue;
        }
        if (configReads[index].clusterId != clusterId || configReads[index].attributeId != attributeId) {
            continue;
        }
        if (haveValueOut != nullptr) {
            *haveValueOut = configReads[index].haveValue;
        }
        if (valueOut != nullptr) {
            *valueOut = configReads[index].value;
        }
        return true;
    }
    return false;
}

bool ZigbeeSpiProxy::sendClusterCommand(
    const uint8_t ieee[8],
    uint8_t endpoint,
    uint16_t clusterId,
    uint8_t commandId,
    const uint8_t *payload,
    uint8_t payloadLength
) {
    uint8_t frame[SPI_MAX_PAYLOAD];
    if (!spiPackZclCommand(
            frame,
            sizeof(frame),
            ieee,
            endpoint,
            clusterId,
            commandId,
            payload,
            payloadLength
        )) {
        return false;
    }
    const bool queued = INTER_CHIP_HOST.tryEnqueue(
        SpiCmdZclCommand,
        frame,
        spiZclCommandFrameLength(payloadLength)
    );
    if (queued) {
        packetsTx++;
    }
    return queued;
}

void ZigbeeSpiProxy::clearPendingDeleteIeees() {
    while (pendingDeleteHead != nullptr) {
        PendingDeleteIeeeNode *next = pendingDeleteHead->next;
        free(pendingDeleteHead);
        pendingDeleteHead = next;
    }
    pendingDeleteCount = 0;
    pendingDeleteIndex = 0;
}

void ZigbeeSpiProxy::queueDeletedIeeesAndPushAll(const uint8_t (*deletedIeees)[8], int deletedCount) {
    clearPendingDeleteIeees();
    pendingUpsertWalk = 0;
    if (deletedIeees != nullptr) {
        const int bounded = deletedCount > DEVICE_MAP_SLOTS ? DEVICE_MAP_SLOTS : deletedCount;
        PendingDeleteIeeeNode *tail = nullptr;
        for (int i = 0; i < bounded; i++) {
            PendingDeleteIeeeNode *node = (PendingDeleteIeeeNode *)malloc(sizeof(PendingDeleteIeeeNode));
            if (node == nullptr) {
                break;
            }
            memcpy(node->ieee, deletedIeees[i], 8);
            node->next = nullptr;
            if (tail == nullptr) {
                pendingDeleteHead = node;
            } else {
                tail->next = node;
            }
            tail = node;
            pendingDeleteCount++;
        }
    }
    fullPushActive = true;
}

void ZigbeeSpiProxy::setRegistryPullDoneHandler(void (*handler)()) {
    registryPullDone = handler;
}

void ZigbeeSpiProxy::setDeviceLiveChangedHandler(DeviceLiveChangedFn handler) {
    deviceLiveChanged = handler;
}

void ZigbeeSpiProxy::setStatusValueChangedHandler(StatusValueChangedFn handler) {
    statusValueChanged = handler;
}

bool ZigbeeSpiProxy::copyEndpointState(
    const uint8_t ieee[8],
    uint8_t endpoint,
    char *out,
    size_t outLen
) const {
    if (out == nullptr || outLen == 0 || ieee == nullptr) {
        return false;
    }
    out[0] = '\0';
    const CachedDevice *slot = findByIeee(ieee);
    if (slot == nullptr || slot->channels == nullptr) {
        return false;
    }
    const EndpointStatus *best = nullptr;
    for (uint8_t index = 0; index < slot->channelCapacity; index++) {
        const EndpointStatus *candidate = &slot->channels[index];
        if (candidate->state == nullptr || candidate->state[0] == '\0') {
            continue;
        }
        if (endpoint == 0) {
            if (best == nullptr || candidate->endpoint < best->endpoint) {
                best = candidate;
            }
            continue;
        }
        if (candidate->endpoint == endpoint) {
            best = candidate;
            break;
        }
    }
    if (best == nullptr || best->state == nullptr) {
        return false;
    }
    strncpy(out, best->state, outLen - 1);
    out[outLen - 1] = '\0';
    return true;
}

bool ZigbeeSpiProxy::copyEndpointStateForParam(
    const uint8_t ieee[8],
    uint8_t endpoint,
    const char *param,
    char *out,
    size_t outLen
) const {
    if (out == nullptr || outLen == 0 || ieee == nullptr) {
        return false;
    }
    out[0] = '\0';
    const CachedDevice *slot = findByIeee(ieee);
    if (slot == nullptr) {
        return false;
    }
    if (param != nullptr && strcmp(param, "battery") == 0) {
        if (!slot->hasBattery) {
            return false;
        }
        snprintf(out, outLen, "%u", (unsigned)slot->batteryPercent);
        return true;
    }
    if (slot->channels == nullptr) {
        return false;
    }
    const EndpointStatus *best = nullptr;
    for (uint8_t index = 0; index < slot->channelCapacity; index++) {
        const EndpointStatus *candidate = &slot->channels[index];
        if (!endpointStatusOccupied(candidate)) {
            continue;
        }
        if (endpoint == 0) {
            if (best == nullptr || candidate->endpoint < best->endpoint) {
                best = candidate;
            }
            continue;
        }
        if (candidate->endpoint == endpoint) {
            best = candidate;
            break;
        }
    }
    if (best == nullptr) {
        return false;
    }
    const char *source = best->state != nullptr ? best->state : "";
    if (param != nullptr && strcmp(param, "position") == 0
        && best->covering != nullptr && best->covering->position[0] != '\0') {
        source = best->covering->position;
    } else if (param != nullptr && strcmp(param, "status") == 0 && best->covering != nullptr) {
        if (best->covering->move[0] != '\0') {
            source = best->covering->move;
        } else if (best->covering->discrete[0] != '\0') {
            source = best->covering->discrete;
        }
    }
    if (source[0] == '\0') {
        return false;
    }
    strncpy(out, source, outLen - 1);
    out[outLen - 1] = '\0';
    return true;
}

void ZigbeeSpiProxy::notifyDeviceLiveChanged(const uint8_t ieee[8]) {
    if (deviceLiveChanged != nullptr && ieee != nullptr) {
        deviceLiveChanged(ieee);
    }
}

void ZigbeeSpiProxy::pumpOnlineWindows() {
    const unsigned long nowMs = millis();
    for (CachedDevice *slot = deviceHead; slot != nullptr; slot = slot->next) {
        const bool nowOnline =
            slot->lastSeenMs != 0 && (long)(nowMs - slot->lastSeenMs) < (long)kOnlineWindowMs;
        if (nowOnline == slot->consoleOnlineSent) {
            continue;
        }
        slot->consoleOnlineSent = nowOnline;
        notifyDeviceLiveChanged(slot->ieee);
    }
}

bool ZigbeeSpiProxy::registryHydrated() const {
    return registryReady;
}

void ZigbeeSpiProxy::requestDevicesFile() {
    filePullRequested = true;
}

String ZigbeeSpiProxy::devicesFileJson() const {
    return fileCache.length() > 0 ? fileCache : String("[]");
}

void ZigbeeSpiProxy::applyDevicesFile(const SpiFrame &frame) {
    if (frame.length < 1) {
        return;
    }
    const uint8_t flags = frame.payload[0];
    const char *chunk = (const char *)(frame.payload + 1);
    const int chunkLength = (int)frame.length - 1;
    if ((flags & SPI_FILE_FIRST) != 0) {
        filePullBuffer = "";
        filePullCollecting = true;
    }
    if (!filePullCollecting && (flags & SPI_FILE_FIRST) == 0) {
        filePullBuffer = "";
        filePullCollecting = true;
    }
    if (chunkLength > 0) {
        filePullBuffer.concat(chunk, (unsigned int)chunkLength);
    }
    if ((flags & SPI_FILE_LAST) != 0) {
        fileCache = filePullBuffer;
        filePullBuffer = "";
        filePullCollecting = false;
        filePullInFlight = false;
        filePullStartedMs = 0;
        LOGGER.info("Received slave devices.json (" + String(fileCache.length()) + " bytes)");
    }
}

void ZigbeeSpiProxy::requestRegistryPull(DeviceTopicMap *topicMap) {
    if (topicMap == nullptr) {
        return;
    }
    registryMap = topicMap;
    if (registryReady || pullCollecting) {
        return;
    }
    if (registryPullStartedMs == 0) {
        registryPullStartedMs = millis();
        LOGGER.info("Waiting for slave device list after radio start");
    }
}

void ZigbeeSpiProxy::publishHostStores(DeviceTopicMap *topicMap, UserStore *store) {
    retainRegisteredTelemetry(topicMap);
    userMap = store;
    registryPullRequested = false;
    registryPullActive = false;
    pullCollecting = false;
    userPullRequested = false;
    userPullActive = false;
    userCollecting = false;
    registryPullStartedMs = 0;
    userPullStartedMs = 0;
    registryReady = true;
    userReady = true;
    queueDeletedIeeesAndPushAll(nullptr, 0);
    queueDeletedUserNamesAndPushAll(nullptr, 0);
    LOGGER.info("Publishing host device and user stores to slave");
}

void ZigbeeSpiProxy::requestUsersPull(UserStore *store) {
    if (store == nullptr) {
        return;
    }
    userMap = store;
    if (userReady || userCollecting) {
        return;
    }
    userPullRequested = true;
    if (userPullStartedMs == 0) {
        userPullStartedMs = millis();
        LOGGER.info("Waiting for slave user list after radio start");
    }
}

bool ZigbeeSpiProxy::queueDeviceChange(uint8_t flags, const DeviceTopicEntry *entry) {
    if (entry == nullptr) {
        return false;
    }
    for (int i = 0; i < kPendingChangeSlots; i++) {
        if (pendingChanges[i].used) {
            continue;
        }
        DeviceTopicEntry *copy = (DeviceTopicEntry *)calloc(1, sizeof(DeviceTopicEntry));
        if (copy == nullptr || !DeviceTopicMap::cloneEntry(copy, entry)) {
            DeviceTopicMap::freeEntry(copy);
            return false;
        }
        pendingChanges[i].used = true;
        pendingChanges[i].flags = flags;
        pendingChanges[i].entry = copy;
        return true;
    }
    LOGGER.warning("Only one device record change at a time");
    return false;
}

bool ZigbeeSpiProxy::enqueueDeviceUpsert(const DeviceTopicEntry *entry) {
    if (entry == nullptr || !entry->used) {
        return false;
    }
    CachedDevice *slot = findByIeee(entry->ieee);
    if (slot != nullptr) {
        ensureChannelCapacity(slot, channelCapacityForIeee(entry->ieee));
    }
    if (entry->transport == DeviceTransportMqtt) {
        return true;
    }
    return queueDeviceChange(SPI_DEVICE_SYNC_ENTRY, entry);
}

void ZigbeeSpiProxy::forgetDeviceTelemetry(const uint8_t ieee[8]) {
    if (ieee == nullptr) {
        return;
    }
    CachedDevice **link = &deviceHead;
    while (*link != nullptr) {
        CachedDevice *slot = *link;
        if (memcmp(slot->ieee, ieee, 8) == 0) {
            *link = slot->next;
            releaseSlot(slot);
            return;
        }
        link = &slot->next;
    }
}

bool ZigbeeSpiProxy::enqueueDeviceDelete(const uint8_t ieee[8]) {
    if (ieee == nullptr) {
        return false;
    }
    forgetDeviceTelemetry(ieee);
    DeviceTopicEntry entry{};
    memcpy(entry.ieee, ieee, 8);
    entry.used = 1;
    return queueDeviceChange(SPI_DEVICE_SYNC_DELETE, &entry);
}

void ZigbeeSpiProxy::applyPendingChangeToMap(const PendingDeviceChange *change) {
    if (registryMap == nullptr || change == nullptr) {
        return;
    }
    if (change->entry == nullptr) {
        return;
    }
    if ((change->flags & SPI_DEVICE_SYNC_DELETE) != 0) {
        registryMap->removeByIeee(change->entry->ieee);
        return;
    }
    if ((change->flags & SPI_DEVICE_SYNC_ENTRY) != 0) {
        registryMap->upsertFromEntry(change->entry, false);
    }
}

void ZigbeeSpiProxy::replayPendingChanges() {
    for (int i = 0; i < kPendingChangeSlots; i++) {
        if (pendingChanges[i].used) {
            applyPendingChangeToMap(&pendingChanges[i]);
        }
    }
}

void ZigbeeSpiProxy::pumpPendingChanges() {
    if (!commandsAllowed() || registryPullRequested || registryPullActive || pullCollecting) {
        return;
    }
    for (int i = 0; i < kPendingChangeSlots; i++) {
        if (!pendingChanges[i].used) {
            continue;
        }
        if (enqueueRegistryFrame(pendingChanges[i].flags, pendingChanges[i].entry)) {
            DeviceTopicMap::freeEntry(pendingChanges[i].entry);
            pendingChanges[i].entry = nullptr;
            pendingChanges[i].used = false;
        }
        return;
    }
    if (!fullPushActive || registryMap == nullptr) {
        return;
    }
    if (pendingDeleteHead != nullptr && pendingDeleteIndex < pendingDeleteCount) {
        PendingDeleteIeeeNode *node = pendingDeleteHead;
        for (int skip = 0; skip < pendingDeleteIndex && node != nullptr; skip++) {
            node = node->next;
        }
        if (node != nullptr) {
            DeviceTopicEntry deletedEntry{};
            memcpy(deletedEntry.ieee, node->ieee, 8);
            deletedEntry.used = 1;
            if (enqueueRegistryFrame(SPI_DEVICE_SYNC_DELETE, &deletedEntry)) {
                pendingDeleteIndex++;
            }
        }
        return;
    }
    const int nextIndex = registryMap->nextUsedIndex(pendingUpsertWalk);
    if (nextIndex < 0) {
        fullPushActive = false;
        clearPendingDeleteIeees();
        pendingUpsertWalk = 0;
        return;
    }
    DeviceTopicEntry *entry = registryMap->slotAt(nextIndex);
    if (entry != nullptr && entry->transport == DeviceTransportMqtt) {
        pendingUpsertWalk = nextIndex + 1;
        return;
    }
    if (entry != nullptr && enqueueRegistryFrame(SPI_DEVICE_SYNC_ENTRY, entry)) {
        pendingUpsertWalk = nextIndex + 1;
    }
}

bool ZigbeeSpiProxy::enqueueRegistryFrame(uint8_t flags, const DeviceTopicEntry *entry) {
    if ((flags & SPI_DEVICE_SYNC_RESET) != 0) {
        LOGGER.warning("Host cannot replace the full device store");
        return false;
    }
    const bool isUpsert = (flags & SPI_DEVICE_SYNC_ENTRY) != 0;
    const bool isDelete = (flags & SPI_DEVICE_SYNC_DELETE) != 0;
    if (isUpsert == isDelete) {
        LOGGER.warning("Host device change must be one create/update or one delete");
        return false;
    }
    uint8_t payload[DeviceTopicMap::kDeviceSyncEntryLenWithNwk];
    const size_t length = DeviceTopicMap::packSyncPayload(payload, sizeof(payload), flags, entry);
    if (length == 0) {
        return false;
    }
    return INTER_CHIP_HOST.tryEnqueue(SpiCmdSetDevice, payload, (uint16_t)length);
}

bool ZigbeeSpiProxy::queueUserChange(uint8_t flags, const UserRecord *user) {
    if (user == nullptr) {
        return false;
    }
    for (int i = 0; i < kPendingChangeSlots; i++) {
        if (pendingUserChanges[i].used) {
            continue;
        }
        pendingUserChanges[i].used = true;
        pendingUserChanges[i].flags = flags;
        pendingUserChanges[i].user = *user;
        return true;
    }
    LOGGER.warning("Only one user record change at a time");
    return false;
}

bool ZigbeeSpiProxy::enqueueUserUpsert(const UserRecord *user) {
    if (user == nullptr || user->userName[0] == '\0') {
        return false;
    }
    return queueUserChange(SPI_USER_SYNC_ENTRY, user);
}

bool ZigbeeSpiProxy::enqueueUserDelete(const char *userName) {
    if (userName == nullptr || userName[0] == '\0') {
        return false;
    }
    UserRecord user;
    memset(&user, 0, sizeof(user));
    strncpy(user.userName, userName, sizeof(user.userName) - 1);
    return queueUserChange(SPI_USER_SYNC_DELETE, &user);
}

void ZigbeeSpiProxy::queueDeletedUserNamesAndPushAll(const char (*removedNames)[USER_NAME_MAX], int removedCount) {
    pendingUserDeleteCount = 0;
    pendingUserDeleteIndex = 0;
    pendingUserUpsertWalk = 0;
    if (removedNames != nullptr) {
        const int bounded = removedCount > USER_STORE_MAX ? USER_STORE_MAX : removedCount;
        for (int i = 0; i < bounded; i++) {
            strncpy(pendingDeleteUserNames[i], removedNames[i], USER_NAME_MAX - 1);
            pendingDeleteUserNames[i][USER_NAME_MAX - 1] = '\0';
        }
        pendingUserDeleteCount = bounded;
    }
    userFullPushActive = true;
}

void ZigbeeSpiProxy::applyPendingUserChangeToStore(const PendingUserChange *change) {
    if (userMap == nullptr || change == nullptr) {
        return;
    }
    if ((change->flags & SPI_USER_SYNC_DELETE) != 0) {
        userMap->removeByName(change->user.userName, false);
        return;
    }
    if ((change->flags & SPI_USER_SYNC_ENTRY) != 0) {
        userMap->upsertFromRecord(&change->user, false);
    }
}

void ZigbeeSpiProxy::replayPendingUserChanges() {
    for (int i = 0; i < kPendingChangeSlots; i++) {
        if (pendingUserChanges[i].used) {
            applyPendingUserChangeToStore(&pendingUserChanges[i]);
        }
    }
}

void ZigbeeSpiProxy::pumpPendingUserChanges() {
    if (!commandsAllowed() || userPullRequested || userPullActive || userCollecting) {
        return;
    }
    for (int i = 0; i < kPendingChangeSlots; i++) {
        if (!pendingUserChanges[i].used) {
            continue;
        }
        if (enqueueUserFrame(pendingUserChanges[i].flags, &pendingUserChanges[i].user)) {
            pendingUserChanges[i].used = false;
        }
        return;
    }
    if (!userFullPushActive || userMap == nullptr) {
        return;
    }
    if (pendingUserDeleteIndex < pendingUserDeleteCount) {
        UserRecord deletedUser;
        memset(&deletedUser, 0, sizeof(deletedUser));
        strncpy(deletedUser.userName, pendingDeleteUserNames[pendingUserDeleteIndex], sizeof(deletedUser.userName) - 1);
        if (enqueueUserFrame(SPI_USER_SYNC_DELETE, &deletedUser)) {
            pendingUserDeleteIndex++;
        }
        return;
    }
    const int nextIndex = userMap->nextUsedIndex(pendingUserUpsertWalk);
    if (nextIndex < 0) {
        userFullPushActive = false;
        pendingUserDeleteCount = 0;
        pendingUserDeleteIndex = 0;
        pendingUserUpsertWalk = 0;
        return;
    }
    const UserRecord *user = userMap->userAt(nextIndex);
    if (user != nullptr && enqueueUserFrame(SPI_USER_SYNC_ENTRY, user)) {
        pendingUserUpsertWalk = nextIndex + 1;
    }
}

bool ZigbeeSpiProxy::enqueueUserFrame(uint8_t flags, const UserRecord *user) {
    if ((flags & SPI_USER_SYNC_RESET) != 0) {
        LOGGER.warning("Host cannot replace the full user store");
        return false;
    }
    const bool isUpsert = (flags & SPI_USER_SYNC_ENTRY) != 0;
    const bool isDelete = (flags & SPI_USER_SYNC_DELETE) != 0;
    if (isUpsert == isDelete) {
        LOGGER.warning("Host user change must be one create/update or one delete");
        return false;
    }
    uint8_t payload[SPI_USER_SYNC_ENTRY_LEN];
    const size_t length = UserStore::packSyncPayload(payload, sizeof(payload), flags, user);
    if (length == 0) {
        return false;
    }
    return INTER_CHIP_HOST.tryEnqueue(SpiCmdSetUser, payload, (uint16_t)length);
}

void ZigbeeSpiProxy::beginUserPullSnapshot() {
    pullUsers.resetToEmpty();
    userCollecting = true;
    userPullActive = true;
    userPullExpectedCount = -1;
}

void ZigbeeSpiProxy::applyPulledUsers(const SpiFrame &frame) {
    if (userMap == nullptr) {
        return;
    }
    uint8_t flags = 0;
    UserRecord user;
    if (!UserStore::unpackSyncPayload(frame.payload, frame.length, &flags, &user)) {
        return;
    }
    if ((flags & SPI_USER_SYNC_RESET) != 0) {
        beginUserPullSnapshot();
        if (frame.length >= 2) {
            userPullExpectedCount = frame.payload[1];
        }
    }
    if ((flags & SPI_USER_SYNC_ENTRY) != 0) {
        if (!userCollecting) {
            beginUserPullSnapshot();
        }
        pullUsers.upsertFromRecord(&user, false);
    }
    if ((flags & SPI_USER_SYNC_LAST) != 0) {
        finishUsersPull();
    }
}

void ZigbeeSpiProxy::finishUsersPull() {
    const int receivedCount = userCollecting ? pullUsers.userCount() : 0;
    if (userCollecting && userPullExpectedCount >= 0 && receivedCount < userPullExpectedCount) {
        LOGGER.warning(
            "User dump incomplete " + String(receivedCount) + "/" + String(userPullExpectedCount)
        );
        userCollecting = false;
        userPullActive = false;
        userReady = false;
        if (userPullRetries < 5) {
            userPullRetries++;
            userPullRequested = true;
            userPullStartedMs = millis();
        }
        return;
    }
    if (userCollecting) {
        LOGGER.info("Ignoring slave user dump; host store is authoritative");
    }
    userCollecting = false;
    userPullActive = false;
    userPullRequested = false;
    userPullStartedMs = 0;
    userPullRetries = 0;
    userPullExpectedCount = -1;
    replayPendingUserChanges();
    userReady = true;
    LOGGER.info("Pulled " + String(userMap != nullptr ? userMap->userCount() : 0) + " user(s) from slave");
}

void ZigbeeSpiProxy::beginPullSnapshot() {
    pullMap.clearAll();
    pullCollecting = true;
    registryPullActive = true;
    registryPullCount = 0;
    pullExpectedCount = -1;
}

void ZigbeeSpiProxy::applyPulledRegistry(const SpiFrame &frame) {
    if (registryMap == nullptr) {
        return;
    }
    uint8_t flags = 0;
    DeviceTopicEntry entry{};
    if (!DeviceTopicMap::unpackSyncPayload(frame.payload, frame.length, &flags, &entry)) {
        return;
    }
    if ((flags & SPI_DEVICE_SYNC_RESET) != 0) {
        beginPullSnapshot();
        if (frame.length >= 2) {
            pullExpectedCount = frame.payload[1];
        }
    }
    if ((flags & SPI_DEVICE_SYNC_ENTRY) != 0) {
        if (!pullCollecting) {
            beginPullSnapshot();
        }
        pullMap.upsertFromEntry(&entry, false);
        DeviceTopicMap::clearEntryStrings(&entry);
        registryPullCount++;
    }
    if ((flags & SPI_DEVICE_SYNC_LAST) != 0) {
        finishRegistryPull();
    }
}

void ZigbeeSpiProxy::finishRegistryPull() {
    const int receivedCount = pullCollecting ? pullMap.usedCount() : 0;
    if (pullCollecting && pullExpectedCount >= 0 && receivedCount < pullExpectedCount) {
        LOGGER.warning(
            "Device dump incomplete " + String(receivedCount) + "/" + String(pullExpectedCount)
        );
        pullCollecting = false;
        registryPullActive = false;
        registryReady = false;
        if (pullRetries < 5) {
            pullRetries++;
            registryPullRequested = true;
            registryPullStartedMs = millis();
        }
        return;
    }
    if (pullCollecting) {
        LOGGER.info("Ignoring slave device dump; host store is authoritative");
    }
    pullCollecting = false;
    registryPullActive = false;
    registryPullRequested = false;
    registryPullStartedMs = 0;
    pullRetries = 0;
    pullExpectedCount = -1;
    replayPendingChanges();
    registryReady = true;
    if (registryPullDone != nullptr) {
        registryPullDone();
    }
}

void ZigbeeSpiProxy::pumpRegistrySync() {
    filePullRequested = false;
    registryPullRequested = false;
    userPullRequested = false;
    pumpPendingChanges();
    pumpPendingUserChanges();
    pumpOnlineWindows();
}

void ZigbeeSpiProxy::onSpiEvent(const SpiFrame &frame) {
    if (frame.cmd == SpiEvtJoinClosed) {
        pairingOpen = false;
        return;
    }
    if (frame.cmd == SpiEvtDevicesFile) {
        applyDevicesFile(frame);
        return;
    }
    if (frame.cmd == SpiEvtDeviceMap) {
        applyPulledRegistry(frame);
        return;
    }
    if (frame.cmd == SpiEvtUserMap) {
        applyPulledUsers(frame);
        return;
    }
    if (frame.cmd == SpiEvtAttrReport && frame.length >= SPI_ATTR_REPORT_MESSAGE_OFFSET + 1) {
        uint8_t ieee[8];
        memcpy(ieee, frame.payload, 8);
        const uint8_t endpoint = frame.payload[8];
        const uint16_t shortAddr = (uint16_t)frame.payload[9] | ((uint16_t)frame.payload[10] << 8);
        const int8_t rssiDbm = (int8_t)frame.payload[SPI_ATTR_REPORT_RSSI_OFFSET];
        char message[SPI_DEVICE_MESSAGE_MAX];
        memset(message, 0, sizeof(message));
        const size_t copyLength = frame.length - SPI_ATTR_REPORT_MESSAGE_OFFSET;
        const size_t bounded = copyLength >= sizeof(message) ? sizeof(message) - 1 : copyLength;
        memcpy(message, frame.payload + SPI_ATTR_REPORT_MESSAGE_OFFSET, bounded);
        message[sizeof(message) - 1] = '\0';
        unsigned parsedCluster = 0;
        unsigned parsedAttr = 0;
        unsigned long parsedValue = 0;
        if (sscanf(message, "cl=0x%X,attr=0x%X,val=0x%lX", &parsedCluster, &parsedAttr, &parsedValue) == 3
            || sscanf(
                   message,
                   "%*[^ ] cl=0x%X,attr=0x%X,val=0x%lX",
                   &parsedCluster,
                   &parsedAttr,
                   &parsedValue
               ) == 3) {
            noteConfigAttrReply(ieee, (uint16_t)parsedCluster, (uint16_t)parsedAttr, (uint32_t)parsedValue);
        }
        CachedDevice *slot = allocSlot(ieee);
        if (slot != nullptr) {
            slot->endpoint = endpoint;
            slot->shortAddr = shortAddr;
            slot->lastSeenMs = millis();
            slot->lastRssiDbm = rssiDbm;
            slot->hasRssi = true;
            noteReportTelemetry(slot, endpoint, message);
            slot->consoleOnlineSent = true;
            notifyDeviceLiveChanged(ieee);
        }
        packetsRx++;
        if (lightStateHandler != nullptr) {
            // Publish plain comparable values (no "BATTERY " / trailing '%').
            char normalizedMessage[SPI_DEVICE_MESSAGE_MAX];
            const char *publishMessage = message;
            unsigned batteryPercent = 0;
            if (sscanf(message, "BATTERY %u", &batteryPercent) == 1) {
                snprintf(normalizedMessage, sizeof(normalizedMessage), "%u", batteryPercent);
                publishMessage = normalizedMessage;
            } else {
                char *percentEnd = nullptr;
                const unsigned long positionPercent = strtoul(message, &percentEnd, 10);
                if (percentEnd != message && *percentEnd == '%' && percentEnd[1] == '\0') {
                    snprintf(normalizedMessage, sizeof(normalizedMessage), "%lu", positionPercent);
                    publishMessage = normalizedMessage;
                }
            }
            lightStateHandler(publishMessage, ieee, endpoint, shortAddr);
        }
        return;
    }
    if (frame.cmd == SpiEvtDeviceJoin && frame.length >= 75) {
        uint8_t ieee[8];
        memcpy(ieee, frame.payload, 8);
        CachedDevice *slot = allocSlot(ieee);
        packetsRx++;
        if (slot == nullptr) {
            return;
        }
        slot->shortAddr = (uint16_t)frame.payload[8] | ((uint16_t)frame.payload[9] << 8);
        slot->endpoint = frame.payload[10];
        slot->lastSeenMs = millis();
        assignBoundedField(slot->manufacturer, (const char *)frame.payload + 11, 32);
        assignBoundedField(slot->model, (const char *)frame.payload + 43, 32);
        slot->consoleOnlineSent = true;
        notifyDeviceLiveChanged(ieee);
    }
    if (frame.cmd == SpiEvtDeviceLeave) {
        packetsRx++;
    }
}

void ZigbeeSpiProxy::noteSeen(const uint8_t ieee[8]) {
    CachedDevice *slot = findByIeee(ieee);
    if (slot == nullptr) {
        slot = allocSlot(ieee);
    }
    if (slot != nullptr) {
        slot->lastSeenMs = millis();
        slot->consoleOnlineSent = true;
        notifyDeviceLiveChanged(ieee);
    }
}

bool ZigbeeSpiProxy::isOnline(const uint8_t ieee[8]) const {
    if (ieee == nullptr) {
        return false;
    }
    const CachedDevice *slot = findByIeee(ieee);
    if (slot == nullptr || slot->lastSeenMs == 0) {
        return false;
    }
    return (long)(millis() - slot->lastSeenMs) < (long)kOnlineWindowMs;
}

bool ZigbeeSpiProxy::lastRssiDbm(const uint8_t ieee[8], int8_t *rssiDbm) const {
    if (ieee == nullptr || rssiDbm == nullptr) {
        return false;
    }
    const CachedDevice *slot = findByIeee(ieee);
    if (slot == nullptr || !slot->hasRssi) {
        return false;
    }
    *rssiDbm = slot->lastRssiDbm;
    return true;
}

void ZigbeeSpiProxy::noteMqttState(const uint8_t ieee[8], uint8_t endpoint, const char *message) {
    CachedDevice *slot = allocSlot(ieee);
    if (slot == nullptr) {
        LOGGER.warning("Host device cache alloc failed; WiFi state not stored");
        return;
    }
    slot->lastSeenMs = millis();
    noteReportTelemetry(slot, endpoint, message);
    slot->consoleOnlineSent = true;
    notifyDeviceLiveChanged(ieee);
}

void ZigbeeSpiProxy::noteMqttOffline(const uint8_t ieee[8]) {
    CachedDevice *slot = findByIeee(ieee);
    if (slot == nullptr) {
        slot = allocSlot(ieee);
    }
    if (slot == nullptr) {
        return;
    }
    slot->lastSeenMs = 0;
    slot->consoleOnlineSent = false;
    notifyDeviceLiveChanged(ieee);
}

void ZigbeeSpiProxy::appendListTelemetry(const uint8_t ieee[8], PsramJsonBuffer &json) const {
    const CachedDevice *slot = findByIeee(ieee);
    if (slot == nullptr) {
        return;
    }
    if (slot->hasBattery) {
        json += ",\"battery\":";
        json += String((unsigned)slot->batteryPercent);
    }
    if (slot->reportableValues != nullptr && slot->reportableValueCount > 0) {
        json += ",\"reportableValues\":[";
        bool firstReportable = true;
        for (uint8_t index = 0; index < slot->reportableValueCount; index++) {
            const ReportableValueSlot &valueSlot = slot->reportableValues[index];
            if (valueSlot.value == nullptr) {
                continue;
            }
            if (!firstReportable) {
                json += ",";
            }
            firstReportable = false;
            char clusterText[12];
            char attributeText[12];
            snprintf(clusterText, sizeof(clusterText), "0x%04X", (unsigned)valueSlot.cluster);
            snprintf(attributeText, sizeof(attributeText), "0x%04X", (unsigned)valueSlot.attribute);
            json += "{\"ep\":";
            json += String((unsigned)valueSlot.endpoint);
            json += ",\"cluster\":\"";
            json += clusterText;
            json += "\",\"attribute\":\"";
            json += attributeText;
            json += "\",\"value\":\"";
            appendJsonEscaped(json, valueSlot.value, strlen(valueSlot.value) + 1);
            json += "\"}";
        }
        json += "]";
    }
    if (slot->channels == nullptr || slot->channelCapacity == 0) {
        return;
    }
    bool anyStatus = false;
    for (uint8_t i = 0; i < slot->channelCapacity; i++) {
        if (endpointStatusOccupied(&slot->channels[i])) {
            anyStatus = true;
            break;
        }
    }
    if (!anyStatus) {
        return;
    }
    json += ",\"status\":[";
    bool first = true;
    uint8_t lastEndpoint = 0;
    bool haveLastEndpoint = false;
    for (;;) {
        int bestIndex = -1;
        for (uint8_t i = 0; i < slot->channelCapacity; i++) {
            if (!endpointStatusOccupied(&slot->channels[i])) {
                continue;
            }
            if (haveLastEndpoint && slot->channels[i].endpoint <= lastEndpoint) {
                continue;
            }
            if (bestIndex < 0
                || slot->channels[i].endpoint < slot->channels[bestIndex].endpoint) {
                bestIndex = i;
            }
        }
        if (bestIndex < 0) {
            break;
        }
        if (!first) {
            json += ",";
        }
        first = false;
        const EndpointStatus &status = slot->channels[bestIndex];
        json += "{\"ep\":";
        json += String((unsigned)status.endpoint);
        json += ",\"state\":\"";
        appendJsonEscaped(
            json,
            status.state != nullptr ? status.state : "",
            kStatusStateMax
        );
        json += "\"";
        if (status.covering != nullptr && status.covering->move[0] != '\0') {
            json += ",\"move\":\"";
            appendJsonEscaped(json, status.covering->move, sizeof(status.covering->move));
            json += "\"";
        }
        if (status.covering != nullptr && status.covering->position[0] != '\0') {
            json += ",\"position\":\"";
            appendJsonEscaped(
                json,
                status.covering->position,
                sizeof(status.covering->position)
            );
            json += "\"";
        }
        json += "}";
        lastEndpoint = status.endpoint;
        haveLastEndpoint = true;
    }
    json += "]";
}

void ZigbeeSpiProxy::notePacketReceived() {
    packetsRx++;
}

void ZigbeeSpiProxy::notePacketSent() {
    packetsTx++;
}

uint32_t ZigbeeSpiProxy::packetsReceived() const {
    return packetsRx;
}

uint32_t ZigbeeSpiProxy::packetsSent() const {
    return packetsTx;
}

bool ZigbeeSpiProxy::pairingActive() const {
    return pairingOpen;
}

int ZigbeeSpiProxy::onlineCount() const {
    int count = 0;
    for (const CachedDevice *slot = deviceHead; slot != nullptr; slot = slot->next) {
        if (slot->lastSeenMs == 0) {
            continue;
        }
        if ((long)(millis() - slot->lastSeenMs) < (long)kOnlineWindowMs) {
            count++;
        }
    }
    return count;
}

String ZigbeeSpiProxy::devicesJson(DeviceTopicMap *topicMap) {
    String json = "[";
    bool first = true;
    size_t usedCount = 0;
    for (CachedDevice *device = deviceHead; device != nullptr; device = device->next) {
        usedCount++;
    }
    json.reserve(usedCount * 192 + 2);
    for (CachedDevice *device = deviceHead; device != nullptr; device = device->next) {
        appendBoundRadioDeviceJson(
            json,
            first,
            topicMap,
            device->ieee,
            device->shortAddr,
            device->endpoint,
            device->manufacturer,
            device->model
        );
    }
    json += "]";
    return json;
}
