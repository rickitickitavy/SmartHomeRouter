#include "DeviceTopicMap.h"
#include "JsonField.h"
#include "SpiProtocol.h"
#include "Defines.h"
#include "DeviceConfigCatalog.h"
#include "DeviceManufacturer.h"
#include "UserDataPayload.h"
#include "ZigbeeCluster.h"
#include "ZigbeeDeviceType.h"

#include <LittleFS.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static_assert(
    DeviceTopicMap::kDeviceSyncEntryLenWithNwk <= SPI_MAX_PAYLOAD,
    "device sync with nwk must fit SPI payload"
);

static size_t jsonEscapeCap(const char *value) {
    if (value == nullptr) {
        return 0;
    }
    return strlen(value) + 1;
}

DeviceTopicMap::DeviceTopicMap() : head(nullptr) {}

DeviceTopicMap::~DeviceTopicMap() {
    clearAll();
}

DeviceTopicEntry *DeviceTopicMap::first() {
    return head;
}

const DeviceTopicEntry *DeviceTopicMap::first() const {
    return head;
}

DeviceTopicEntry *DeviceTopicMap::nextEntry(const DeviceTopicEntry *entry) {
    return entry != nullptr ? entry->next : nullptr;
}

char *DeviceTopicMap::duplicateBoundedString(const char *source, size_t maxLen) {
    if (maxLen == 0) {
        return nullptr;
    }
    if (source == nullptr) {
        source = "";
    }
    size_t copyLen = strlen(source);
    if (copyLen >= maxLen) {
        copyLen = maxLen - 1;
    }
    char *copy = (char *)malloc(copyLen + 1);
    if (copy == nullptr) {
        return nullptr;
    }
    memcpy(copy, source, copyLen);
    copy[copyLen] = '\0';
    return copy;
}

void DeviceTopicMap::clearEntryStrings(DeviceTopicEntry *entry) {
    if (entry == nullptr) {
        return;
    }
    free(entry->friendlyName);
    free(entry->stateTopic);
    free(entry->commandTopic);
    free(entry->availabilityTopic);
    userDataClearFieldDefs(&entry->userDataFields, &entry->userDataFieldCount);
    deviceCustomCommandClear(&entry->customCommands, &entry->customCommandCount);
    reportableDataClear(&entry->reportableData, &entry->reportableDataCount);
    entry->friendlyName = nullptr;
    entry->stateTopic = nullptr;
    entry->commandTopic = nullptr;
    entry->availabilityTopic = nullptr;
}

void DeviceTopicMap::freeEntry(DeviceTopicEntry *entry) {
    if (entry == nullptr) {
        return;
    }
    clearEntryStrings(entry);
    free(entry);
}

bool DeviceTopicMap::cloneEntry(DeviceTopicEntry *destination, const DeviceTopicEntry *source) {
    if (destination == nullptr || source == nullptr) {
        return false;
    }
    clearEntryStrings(destination);
    memcpy(destination->ieee, source->ieee, 8);
    destination->friendlyName =
        duplicateBoundedString(source->friendlyName, SPI_DEVICE_SYNC_NAME_LEN);
    destination->stateTopic =
        duplicateBoundedString(source->stateTopic, SPI_DEVICE_SYNC_TOPIC_LEN);
    destination->commandTopic =
        duplicateBoundedString(source->commandTopic, SPI_DEVICE_SYNC_TOPIC_LEN);
    destination->availabilityTopic =
        duplicateBoundedString(source->availabilityTopic, SPI_DEVICE_SYNC_TOPIC_LEN);
    if ((source->friendlyName != nullptr && source->friendlyName[0] != '\0' && destination->friendlyName == nullptr)
        || (source->stateTopic != nullptr && source->stateTopic[0] != '\0' && destination->stateTopic == nullptr)
        || (source->commandTopic != nullptr && source->commandTopic[0] != '\0' && destination->commandTopic == nullptr)
        || (source->availabilityTopic != nullptr
            && source->availabilityTopic[0] != '\0'
            && destination->availabilityTopic == nullptr)) {
        clearEntryStrings(destination);
        return false;
    }
    destination->shortAddr = source->shortAddr;
    destination->channelCount = source->channelCount;
    destination->fullControl = source->fullControl;
    destination->debug = source->debug;
    destination->publishToMqtt = source->publishToMqtt;
    destination->zigbeeProtocol = source->zigbeeProtocol;
    destination->zigbeeType = source->zigbeeType;
    destination->manufacturer = source->manufacturer;
    destination->transport = source->transport;
    if (!userDataCopyFieldDefs(
            &destination->userDataFields,
            &destination->userDataFieldCount,
            source->userDataFields,
            source->userDataFieldCount
        )) {
        clearEntryStrings(destination);
        return false;
    }
    if (!deviceCustomCommandCopy(
            &destination->customCommands,
            &destination->customCommandCount,
            source->customCommands,
            source->customCommandCount
        )) {
        clearEntryStrings(destination);
        return false;
    }
    if (!reportableDataCopy(
            &destination->reportableData,
            &destination->reportableDataCount,
            source->reportableData,
            source->reportableDataCount
        )) {
        clearEntryStrings(destination);
        return false;
    }
    destination->used = source->used;
    destination->next = nullptr;
    return true;
}

void DeviceTopicMap::assignEntryStrings(
    DeviceTopicEntry *entry,
    const char *friendlyName,
    const char *stateTopic,
    const char *commandTopic,
    const char *availabilityTopic
) {
    if (entry == nullptr) {
        return;
    }
    free(entry->friendlyName);
    free(entry->stateTopic);
    free(entry->commandTopic);
    free(entry->availabilityTopic);
    entry->friendlyName = duplicateBoundedString(friendlyName, SPI_DEVICE_SYNC_NAME_LEN);
    entry->stateTopic = duplicateBoundedString(stateTopic, SPI_DEVICE_SYNC_TOPIC_LEN);
    entry->commandTopic = duplicateBoundedString(commandTopic, SPI_DEVICE_SYNC_TOPIC_LEN);
    entry->availabilityTopic = duplicateBoundedString(availabilityTopic, SPI_DEVICE_SYNC_TOPIC_LEN);
}

DeviceTopicEntry *DeviceTopicMap::allocateEntry() {
    if (usedCount() >= DEVICE_MAP_SLOTS) {
        return nullptr;
    }
    DeviceTopicEntry *entry = (DeviceTopicEntry *)calloc(1, sizeof(DeviceTopicEntry));
    if (entry == nullptr) {
        return nullptr;
    }
    entry->next = head;
    head = entry;
    return entry;
}

void DeviceTopicMap::unlinkEntry(DeviceTopicEntry *entry) {
    if (entry == nullptr || head == nullptr) {
        return;
    }
    if (head == entry) {
        head = entry->next;
        entry->next = nullptr;
        return;
    }
    DeviceTopicEntry *previous = head;
    while (previous->next != nullptr && previous->next != entry) {
        previous = previous->next;
    }
    if (previous->next == entry) {
        previous->next = entry->next;
        entry->next = nullptr;
    }
}

bool DeviceTopicMap::ieeeEqual(const uint8_t left[8], const uint8_t right[8]) {
    return memcmp(left, right, 8) == 0;
}

bool DeviceTopicMap::isUsableShortAddr(uint16_t shortAddr) {
    return shortAddr != 0 && shortAddr != 0xFFFF;
}

bool DeviceTopicMap::applyLastKnownShort(const uint8_t ieee[8], uint16_t shortAddr) {
    if (ieee == nullptr || !isUsableShortAddr(shortAddr)) {
        return false;
    }
    DeviceTopicEntry *target = findByIeee(ieee);
    if (target == nullptr || !target->used) {
        return false;
    }
    if (clampDeviceTransport(target->transport) != DeviceTransportZigbee) {
        return false;
    }
    bool changed = target->shortAddr != shortAddr;
    target->shortAddr = shortAddr;
    for (DeviceTopicEntry *entry = head; entry != nullptr; entry = entry->next) {
        if (!entry->used || entry == target) {
            continue;
        }
        if (clampDeviceTransport(entry->transport) != DeviceTransportZigbee) {
            continue;
        }
        if (entry->shortAddr == shortAddr) {
            entry->shortAddr = 0;
            changed = true;
        }
    }
    return changed;
}

DeviceTopicEntry *DeviceTopicMap::slotAt(int index) {
    if (index < 0) {
        return nullptr;
    }
    int ordinal = 0;
    for (DeviceTopicEntry *entry = head; entry != nullptr; entry = entry->next) {
        if (!entry->used) {
            continue;
        }
        if (ordinal == index) {
            return entry;
        }
        ordinal++;
    }
    return nullptr;
}

int DeviceTopicMap::slotIndex(const DeviceTopicEntry *entry) const {
    if (entry == nullptr) {
        return -1;
    }
    int index = 0;
    for (const DeviceTopicEntry *cursor = head; cursor != nullptr; cursor = cursor->next, index++) {
        if (cursor == entry) {
            return index;
        }
    }
    return -1;
}

int DeviceTopicMap::usedCount() const {
    int count = 0;
    for (const DeviceTopicEntry *entry = head; entry != nullptr; entry = entry->next) {
        if (entry->used) {
            count++;
        }
    }
    return count;
}

int DeviceTopicMap::uniqueMqttTopicCount() const {
    const char *uniqueTopics[DEVICE_MAP_SLOTS * 2];
    int uniqueCount = 0;
    for (const DeviceTopicEntry *entry = head; entry != nullptr; entry = entry->next) {
        if (!entry->used) {
            continue;
        }
        const char *candidateTopics[2] = {entry->stateTopic, entry->commandTopic};
        for (int topicIndex = 0; topicIndex < 2; topicIndex++) {
            const char *topic = candidateTopics[topicIndex];
            if (deviceTopicEmpty(topic)) {
                continue;
            }
            bool alreadySeen = false;
            for (int seenIndex = 0; seenIndex < uniqueCount; seenIndex++) {
                if (strcmp(uniqueTopics[seenIndex], topic) == 0) {
                    alreadySeen = true;
                    break;
                }
            }
            if (!alreadySeen) {
                uniqueTopics[uniqueCount] = topic;
                uniqueCount++;
            }
        }
    }
    return uniqueCount;
}

DeviceTopicEntry *DeviceTopicMap::findByIeee(const uint8_t ieee[8]) {
    return const_cast<DeviceTopicEntry *>(
        static_cast<const DeviceTopicMap *>(this)->findByIeee(ieee)
    );
}

const DeviceTopicEntry *DeviceTopicMap::findByIeee(const uint8_t ieee[8]) const {
    for (const DeviceTopicEntry *entry = head; entry != nullptr; entry = entry->next) {
        if (entry->used && ieeeEqual(entry->ieee, ieee)) {
            return entry;
        }
    }
    return nullptr;
}

uint8_t DeviceTopicMap::normalizeChannelCount(int raw) {
    if (raw == DEVICE_CHANNEL_PARSE) {
        return DEVICE_CHANNEL_PARSE;
    }
    if (raw >= DEVICE_CHANNEL_COUNT_DEFAULT && raw <= DEVICE_CHANNEL_COUNT_MAX) {
        return (uint8_t)raw;
    }
    return DEVICE_CHANNEL_COUNT_DEFAULT;
}

bool DeviceTopicMap::usesPayloadParse(uint8_t channelCount) {
    return channelCount == DEVICE_CHANNEL_PARSE;
}

bool DeviceTopicMap::usesTopicSuffix(uint8_t channelCount) {
    return channelCount >= 2 && channelCount <= DEVICE_CHANNEL_COUNT_MAX;
}

bool DeviceTopicMap::isUsableEndpoint(uint8_t endpoint) {
    return endpoint >= 1 && endpoint <= 240;
}

String DeviceTopicMap::statePublishTopic(const DeviceTopicEntry *entry, uint8_t endpoint) {
    if (entry == nullptr || deviceTopicEmpty(entry->stateTopic)) {
        return "";
    }
    if (deviceUsesExactMqttTopics(entry)) {
        return String(entry->stateTopic);
    }
    if (usesTopicSuffix(entry->channelCount) && isUsableEndpoint(endpoint)) {
        return String(entry->stateTopic) + "/" + String(endpoint);
    }
    return String(entry->stateTopic);
}

String DeviceTopicMap::commandPublishTopic(const DeviceTopicEntry *entry, uint8_t endpoint) {
    if (entry == nullptr || deviceTopicEmpty(entry->commandTopic)) {
        return "";
    }
    if (deviceUsesExactMqttTopics(entry)) {
        return String(entry->commandTopic);
    }
    if (usesTopicSuffix(entry->channelCount) && isUsableEndpoint(endpoint)) {
        return String(entry->commandTopic) + "/" + String(endpoint);
    }
    return String(entry->commandTopic);
}

String DeviceTopicMap::statePublishPayload(const DeviceTopicEntry *entry, uint8_t endpoint, const char *message) {
    const char *body = message != nullptr ? message : "";
    if (entry != nullptr
        && !deviceUsesExactMqttTopics(entry)
        && usesPayloadParse(entry->channelCount)
        && isUsableEndpoint(endpoint)) {
        return String("ch-") + String(endpoint) + "##" + body;
    }
    return String(body);
}

bool DeviceTopicMap::parseChannelPayload(const char *payload, uint8_t *endpoint, String *action) {
    if (payload == nullptr || endpoint == nullptr || action == nullptr) {
        return false;
    }
    if (strncmp(payload, "ch-", 3) != 0) {
        return false;
    }
    const char *digits = payload + 3;
    if (*digits < '0' || *digits > '9') {
        return false;
    }
    const int parsed = atoi(digits);
    const char *separator = strstr(digits, "##");
    if (separator == nullptr || parsed < 1 || parsed > 240) {
        return false;
    }
    *endpoint = (uint8_t)parsed;
    *action = String(separator + 2);
    return true;
}

static bool bodyHasKeyEquals(const char *body, const char *key) {
    if (body == nullptr || key == nullptr || key[0] == '\0') {
        return false;
    }
    const size_t keyLen = strlen(key);
    const char *cursor = body;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t' || *cursor == ',') {
            cursor++;
        }
        if (strncasecmp(cursor, key, keyLen) == 0 && cursor[keyLen] == '=') {
            return true;
        }
        const char *nextComma = strchr(cursor, ',');
        if (nextComma == nullptr) {
            break;
        }
        cursor = nextComma + 1;
    }
    return false;
}

static bool bodyLooksZigbeeShaped(const char *body) {
    return bodyHasKeyEquals(body, "cl") || bodyHasKeyEquals(body, "cmd");
}

static String ensureProtocolEndpoint(const String &body, uint8_t endpoint) {
    if (bodyHasKeyEquals(body.c_str(), "ep")) {
        return body;
    }
    const uint8_t usable = DeviceTopicMap::isUsableEndpoint(endpoint) ? endpoint : 1;
    if (body.length() == 0) {
        return String("ep=") + String((unsigned)usable);
    }
    return body + ",ep=" + String((unsigned)usable);
}

uint8_t DeviceTopicMap::endpointFromProtocolBody(const char *body, uint8_t fallbackEndpoint) {
    const uint8_t fallback = isUsableEndpoint(fallbackEndpoint) ? fallbackEndpoint : 1;
    if (body == nullptr || body[0] == '\0') {
        return fallback;
    }
    ZclWriteFields writeFields;
    if (parseFullControlBody(body, fallback, &writeFields) && writeFields.parsedAny) {
        if (isUsableEndpoint(writeFields.endpoint)) {
            return writeFields.endpoint;
        }
    }
    ZclCommandFields commandFields;
    if (parseZclCommandBody(body, fallback, &commandFields) && commandFields.parsed) {
        if (isUsableEndpoint(commandFields.endpoint)) {
            return commandFields.endpoint;
        }
    }
    return fallback;
}

String DeviceTopicMap::encodeZigbeeProtocolCommand(
    const DeviceTopicEntry *entry,
    const char *message,
    uint8_t endpoint
) {
    const uint8_t usable = isUsableEndpoint(endpoint) ? endpoint : 1;
    String body = String(message != nullptr ? message : "");
    body.trim();
    if (body.length() == 0) {
        return ensureProtocolEndpoint("", usable);
    }
    if (bodyLooksZigbeeShaped(body.c_str())) {
        return ensureProtocolEndpoint(body, usable);
    }

    if (entry != nullptr && zigbeeDeviceTypeIsUserData(entry->zigbeeType)) {
        // Only brace-list payloads use the user-data cluster (0xFC00). Other
        // shortcuts / already-shaped bodies keep their own cluster ids.
        const char *braceCursor = body.c_str();
        while (*braceCursor == ' ' || *braceCursor == '\t') {
            braceCursor++;
        }
        if (*braceCursor == '{') {
            String wrapped = "cl=0x";
            char clusterHex[8];
            snprintf(clusterHex, sizeof(clusterHex), "%04X", (unsigned)kUserDataClusterId);
            wrapped += clusterHex;
            wrapped += ",attr=0x";
            char attrHex[8];
            snprintf(attrHex, sizeof(attrHex), "%04X", (unsigned)kUserDataAttributeId);
            wrapped += attrHex;
            wrapped += ",val=";
            wrapped += body;
            return ensureProtocolEndpoint(wrapped, usable);
        }
    }

    String shortcut = body;
    shortcut.toLowerCase();
    char encoded[96];
    if (shortcut == "on") {
        snprintf(
            encoded,
            sizeof(encoded),
            "cl=0x%04X,attr=0x0000,val=0x1,ep=%u",
            (unsigned)kZigbeeClusterOnOff,
            (unsigned)usable
        );
        return String(encoded);
    }
    if (shortcut == "off") {
        snprintf(
            encoded,
            sizeof(encoded),
            "cl=0x%04X,attr=0x0000,val=0x0,ep=%u",
            (unsigned)kZigbeeClusterOnOff,
            (unsigned)usable
        );
        return String(encoded);
    }
    if (shortcut == "toggle") {
        snprintf(
            encoded,
            sizeof(encoded),
            "cl=0x%04X,cmd=0x02,ep=%u",
            (unsigned)kZigbeeClusterOnOff,
            (unsigned)usable
        );
        return String(encoded);
    }

    ZclCommandFields commandFields;
    if (parseZclCommandBody(body.c_str(), usable, &commandFields) && commandFields.parsed) {
        if (commandFields.payloadLength == 0) {
            snprintf(
                encoded,
                sizeof(encoded),
                "cl=0x%04X,cmd=0x%02X,ep=%u",
                (unsigned)commandFields.clusterId,
                (unsigned)commandFields.commandId,
                (unsigned)usable
            );
            return String(encoded);
        }
        if (commandFields.clusterId == kZigbeeClusterWindowCovering
            && commandFields.commandId == kZigbeeWindowCoveringCmdGoToLiftPercentage
            && commandFields.payloadLength >= 1) {
            snprintf(
                encoded,
                sizeof(encoded),
                "cl=0x%04X,cmd=0x%02X,val=%u,ep=%u",
                (unsigned)commandFields.clusterId,
                (unsigned)commandFields.commandId,
                (unsigned)commandFields.payload[0],
                (unsigned)usable
            );
            return String(encoded);
        }
        if (commandFields.clusterId == kZigbeeClusterLevelControl
            && commandFields.payloadLength >= 1) {
            snprintf(
                encoded,
                sizeof(encoded),
                "cl=0x%04X,cmd=0x%02X,val=%u,ep=%u",
                (unsigned)commandFields.clusterId,
                (unsigned)commandFields.commandId,
                (unsigned)commandFields.payload[0],
                (unsigned)usable
            );
            return String(encoded);
        }
        snprintf(
            encoded,
            sizeof(encoded),
            "cl=0x%04X,cmd=0x%02X,ep=%u",
            (unsigned)commandFields.clusterId,
            (unsigned)commandFields.commandId,
            (unsigned)usable
        );
        return String(encoded);
    }
    return body;
}

String DeviceTopicMap::normalizeZigbeeProtocolStatus(const char *message) {
    if (message == nullptr || message[0] == '\0') {
        return String();
    }
    // Prefer key=value parse so decimal val=70 works (not only val=0x…).
    // Apply semantics from the packet's cl=/attr=, not from device type.
    ZclWriteFields writeFields;
    if (parseFullControlBody(message, 1, &writeFields)
        && writeFields.parsedAny
        && writeFields.hasWrite) {
        if (writeFields.clusterId == kZigbeeClusterOnOff && writeFields.attributeId == 0x0000) {
            return writeFields.attributeValue != 0 ? String("ON") : String("OFF");
        }
        if (writeFields.clusterId == kZigbeeClusterIasZone
            && writeFields.attributeId == kZigbeeAttrIasZoneStatus) {
            return (writeFields.attributeValue & 0x1u) != 0 ? String("LEAK") : String("DRY");
        }
        if (writeFields.clusterId == kZigbeeClusterWindowCovering
            && writeFields.attributeId == kZigbeeAttrCurrentPositionLiftPercentage) {
            return String((unsigned)writeFields.attributeValue);
        }
        if (writeFields.clusterId == kZigbeeClusterWindowCovering
            && writeFields.attributeId == kZigbeeAttrWindowCoveringMoveStatus) {
            if (writeFields.attributeValue == kZigbeeWindowCoveringMoveDown) {
                return String("DOWN");
            }
            if (writeFields.attributeValue == kZigbeeWindowCoveringMoveUp) {
                return String("UP");
            }
            return String("STOPPED");
        }
        if (writeFields.clusterId == kZigbeeClusterPowerConfig
            && writeFields.attributeId == kZigbeeAttrBatteryPercentageRemaining) {
            // MQTT often sends human % (0–100). ZCL raw is 0–200 in half-percent units.
            unsigned percent = 0;
            if (writeFields.attributeValue <= 100u) {
                percent = (unsigned)writeFields.attributeValue;
            } else if (zigbeeBatteryPercentageRemainingValid(writeFields.attributeValue)) {
                percent = zigbeeBatteryPercentageFromRemaining(writeFields.attributeValue);
            } else {
                percent = 100;
            }
            char batteryMessage[24];
            snprintf(batteryMessage, sizeof(batteryMessage), "BATTERY %u", percent);
            return String(batteryMessage);
        }
        // userData (0xFC00) and other clusters: keep the shaped body so
        // noteReportTelemetry can dispatch by cl= from the packet.
        return String(message);
    }
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
        if (parsedCluster == kZigbeeClusterOnOff && parsedAttr == 0x0000) {
            return parsedValue != 0 ? String("ON") : String("OFF");
        }
        if (parsedCluster == kZigbeeClusterIasZone && parsedAttr == kZigbeeAttrIasZoneStatus) {
            return (parsedValue & 0x1u) != 0 ? String("LEAK") : String("DRY");
        }
        if (parsedCluster == kZigbeeClusterWindowCovering
            && parsedAttr == kZigbeeAttrCurrentPositionLiftPercentage) {
            return String((unsigned)parsedValue);
        }
        if (parsedCluster == kZigbeeClusterWindowCovering
            && parsedAttr == kZigbeeAttrWindowCoveringMoveStatus) {
            if (parsedValue == kZigbeeWindowCoveringMoveDown) {
                return String("DOWN");
            }
            if (parsedValue == kZigbeeWindowCoveringMoveUp) {
                return String("UP");
            }
            return String("STOPPED");
        }
        if (parsedCluster == kZigbeeClusterPowerConfig
            && parsedAttr == kZigbeeAttrBatteryPercentageRemaining) {
            unsigned percent = 0;
            if (parsedValue <= 100u) {
                percent = (unsigned)parsedValue;
            } else if (zigbeeBatteryPercentageRemainingValid((uint32_t)parsedValue)) {
                percent = zigbeeBatteryPercentageFromRemaining((uint32_t)parsedValue);
            } else {
                percent = 100;
            }
            char batteryMessage[24];
            snprintf(batteryMessage, sizeof(batteryMessage), "BATTERY %u", percent);
            return String(batteryMessage);
        }
    }
    return String(message);
}

static bool parseNumericField(const String &text, uint32_t *outValue) {
    if (outValue == nullptr) {
        return false;
    }
    String trimmed = text;
    trimmed.trim();
    if (trimmed.length() == 0) {
        return false;
    }
    char *endPointer = nullptr;
    unsigned long parsed = strtoul(trimmed.c_str(), &endPointer, 0);
    if (endPointer == trimmed.c_str()) {
        return false;
    }
    *outValue = (uint32_t)parsed;
    return true;
}

static uint8_t dataTypeFromValue(uint32_t attributeValue) {
    if (attributeValue <= 0xFFu) {
        return ZCL_ATTR_TYPE_U8;
    }
    if (attributeValue <= 0xFFFFu) {
        return ZCL_ATTR_TYPE_U16;
    }
    return ZCL_ATTR_TYPE_U32;
}

bool DeviceTopicMap::parseFullControlBody(const char *body, uint8_t mappedEndpoint, ZclWriteFields *fields) {
    if (fields == nullptr) {
        return false;
    }
    fields->clusterId = 0x0006;
    fields->attributeId = 0x0000;
    fields->attributeValue = 0;
    fields->endpoint = mappedEndpoint;
    fields->dataType = ZCL_ATTR_TYPE_U8;
    fields->parsedAny = false;
    fields->hasWrite = false;
    if (body == nullptr) {
        return true;
    }

    String remaining = String(body);
    remaining.trim();
    bool sawType = false;
    while (remaining.length() > 0) {
        const int commaIndex = remaining.indexOf(',');
        String token = commaIndex >= 0 ? remaining.substring(0, commaIndex) : remaining;
        remaining = commaIndex >= 0 ? remaining.substring(commaIndex + 1) : "";
        token.trim();
        const int equalsIndex = token.indexOf('=');
        if (equalsIndex <= 0) {
            continue;
        }
        String key = token.substring(0, equalsIndex);
        String fieldValue = token.substring(equalsIndex + 1);
        key.trim();
        key.toLowerCase();
        fieldValue.trim();
        uint32_t parsedNumber = 0;
        if (key == "cl") {
            if (!parseNumericField(fieldValue, &parsedNumber)) {
                continue;
            }
            fields->clusterId = (uint16_t)parsedNumber;
            fields->parsedAny = true;
        } else if (key == "attr") {
            if (!parseNumericField(fieldValue, &parsedNumber)) {
                continue;
            }
            fields->attributeId = (uint16_t)parsedNumber;
            fields->parsedAny = true;
            fields->hasWrite = true;
        } else if (key == "val") {
            if (!parseNumericField(fieldValue, &parsedNumber)) {
                continue;
            }
            fields->attributeValue = parsedNumber;
            fields->parsedAny = true;
            fields->hasWrite = true;
        } else if (key == "ep") {
            if (!parseNumericField(fieldValue, &parsedNumber)) {
                continue;
            }
            const uint8_t parsedEndpoint = (uint8_t)parsedNumber;
            if (isUsableEndpoint(parsedEndpoint)) {
                fields->endpoint = parsedEndpoint;
            }
            fields->parsedAny = true;
        } else if (key == "type") {
            if (!parseNumericField(fieldValue, &parsedNumber)) {
                continue;
            }
            fields->dataType = (uint8_t)parsedNumber;
            sawType = true;
            fields->parsedAny = true;
        }
    }
    if (!sawType) {
        fields->dataType = dataTypeFromValue(fields->attributeValue);
    }
    return true;
}

static bool parseHexPayload(const String &text, uint8_t *out, uint8_t *outLength, size_t outMax) {
    if (out == nullptr || outLength == nullptr) {
        return false;
    }
    int highNibble = -1;
    uint8_t filled = 0;
    for (unsigned index = 0; index < text.length(); index++) {
        const char character = text.charAt(index);
        if (character == ' ' || character == ':' || character == '-') {
            continue;
        }
        int nibble = -1;
        if (character >= '0' && character <= '9') {
            nibble = character - '0';
        } else if (character >= 'a' && character <= 'f') {
            nibble = 10 + (character - 'a');
        } else if (character >= 'A' && character <= 'F') {
            nibble = 10 + (character - 'A');
        } else {
            return false;
        }
        if (highNibble < 0) {
            highNibble = nibble;
            continue;
        }
        if (filled >= outMax) {
            return false;
        }
        out[filled++] = (uint8_t)((highNibble << 4) | nibble);
        highNibble = -1;
    }
    if (highNibble >= 0) {
        return false;
    }
    *outLength = filled;
    return true;
}

bool DeviceTopicMap::parseZclCommandBody(const char *body, uint8_t mappedEndpoint, ZclCommandFields *fields) {
    if (fields == nullptr) {
        return false;
    }
    memset(fields, 0, sizeof(*fields));
    fields->clusterId = kZigbeeClusterWindowCovering;
    fields->endpoint = mappedEndpoint;
    if (body == nullptr) {
        return true;
    }

    String trimmed = String(body);
    trimmed.trim();
    String shortcut = trimmed;
    shortcut.toLowerCase();
    if (shortcut == "open" || shortcut == "up") {
        fields->commandId = kZigbeeWindowCoveringCmdOpen;
        fields->parsed = true;
        return true;
    }
    if (shortcut == "close" || shortcut == "down") {
        fields->commandId = kZigbeeWindowCoveringCmdClose;
        fields->parsed = true;
        return true;
    }
    if (shortcut == "stop") {
        fields->commandId = kZigbeeWindowCoveringCmdStop;
        fields->parsed = true;
        return true;
    }
    if (shortcut.startsWith("moveto")) {
        String valueText = trimmed.substring(6);
        valueText.trim();
        if (valueText.startsWith("=") || valueText.startsWith(":")) {
            valueText = valueText.substring(1);
            valueText.trim();
        }
        uint32_t percent = 0;
        if (parseNumericField(valueText, &percent) && percent <= 100) {
            fields->clusterId = kZigbeeClusterWindowCovering;
            fields->commandId = kZigbeeWindowCoveringCmdGoToLiftPercentage;
            fields->payload[0] = (uint8_t)percent;
            fields->payloadLength = 1;
            fields->parsed = true;
            return true;
        }
    }
    if (shortcut.startsWith("level")) {
        String valueText = trimmed.substring(5);
        valueText.trim();
        if (valueText.startsWith("=") || valueText.startsWith(":")) {
            valueText = valueText.substring(1);
            valueText.trim();
        }
        uint32_t level = 0;
        if (parseNumericField(valueText, &level) && level <= 254) {
            fields->clusterId = kZigbeeClusterLevelControl;
            fields->commandId = kZigbeeLevelControlCmdMoveToLevel;
            fields->payload[0] = (uint8_t)level;
            fields->payload[1] = 0xFF;
            fields->payload[2] = 0xFF;
            fields->payloadLength = 3;
            fields->parsed = true;
            return true;
        }
    }

    String remaining = trimmed;
    bool sawCluster = false;
    bool sawCommand = false;
    while (remaining.length() > 0) {
        const int commaIndex = remaining.indexOf(',');
        String token = commaIndex >= 0 ? remaining.substring(0, commaIndex) : remaining;
        remaining = commaIndex >= 0 ? remaining.substring(commaIndex + 1) : "";
        token.trim();
        const int equalsIndex = token.indexOf('=');
        if (equalsIndex <= 0) {
            continue;
        }
        String key = token.substring(0, equalsIndex);
        String fieldValue = token.substring(equalsIndex + 1);
        key.trim();
        key.toLowerCase();
        fieldValue.trim();
        uint32_t parsedNumber = 0;
        if (key == "cl") {
            if (!parseNumericField(fieldValue, &parsedNumber)) {
                continue;
            }
            fields->clusterId = (uint16_t)parsedNumber;
            sawCluster = true;
        } else if (key == "cmd") {
            if (!parseNumericField(fieldValue, &parsedNumber)) {
                continue;
            }
            fields->commandId = (uint8_t)parsedNumber;
            sawCommand = true;
        } else if (key == "ep") {
            if (!parseNumericField(fieldValue, &parsedNumber)) {
                continue;
            }
            const uint8_t parsedEndpoint = (uint8_t)parsedNumber;
            if (isUsableEndpoint(parsedEndpoint)) {
                fields->endpoint = parsedEndpoint;
            }
        } else if (key == "pl" || key == "payload") {
            if (!parseHexPayload(
                    fieldValue,
                    fields->payload,
                    &fields->payloadLength,
                    sizeof(fields->payload)
                )) {
                continue;
            }
        }
    }
    fields->parsed = sawCluster && sawCommand;
    return true;
}

static bool mappedTopicEquals(const char *mappedTopic, const char *topic) {
    while (*mappedTopic == ' ' || *mappedTopic == '\t') {
        mappedTopic++;
    }
    while (*topic == ' ' || *topic == '\t') {
        topic++;
    }
    while (*mappedTopic != '\0' && *topic != '\0' && *mappedTopic == *topic) {
        mappedTopic++;
        topic++;
    }
    while (*mappedTopic == ' ' || *mappedTopic == '\t') {
        mappedTopic++;
    }
    while (*topic == ' ' || *topic == '\t' || *topic == '\r' || *topic == '\n') {
        topic++;
    }
    return *mappedTopic == '\0' && *topic == '\0';
}

static DeviceTopicEntry *findByMappedTopic(
    DeviceTopicEntry *listHead,
    const char *topic,
    uint8_t *topicEndpoint,
    bool useStateTopic
) {
    if (topic == nullptr || topic[0] == '\0') {
        return nullptr;
    }
    if (topicEndpoint != nullptr) {
        *topicEndpoint = 0;
    }
    for (DeviceTopicEntry *entry = listHead; entry != nullptr; entry = entry->next) {
        if (!entry->used) {
            continue;
        }
        const char *mappedTopic = useStateTopic ? entry->stateTopic : entry->commandTopic;
        if (deviceTopicEmpty(mappedTopic)) {
            continue;
        }
        if (mappedTopicEquals(mappedTopic, topic)) {
            return entry;
        }
        if (deviceUsesExactMqttTopics(entry) || !DeviceTopicMap::usesTopicSuffix(entry->channelCount)) {
            continue;
        }
        const size_t prefixLength = strlen(mappedTopic);
        if (strncmp(topic, mappedTopic, prefixLength) != 0 || topic[prefixLength] != '/') {
            continue;
        }
        const int parsed = atoi(topic + prefixLength + 1);
        if (!DeviceTopicMap::isUsableEndpoint((uint8_t)parsed)) {
            continue;
        }
        if (topicEndpoint != nullptr) {
            *topicEndpoint = (uint8_t)parsed;
        }
        return entry;
    }
    return nullptr;
}

DeviceTopicEntry *DeviceTopicMap::findByCommandTopic(const char *topic) {
    return findByCommandTopic(topic, nullptr);
}

DeviceTopicEntry *DeviceTopicMap::findByCommandTopic(const char *topic, uint8_t *topicEndpoint) {
    return findByMappedTopic(head, topic, topicEndpoint, false);
}

DeviceTopicEntry *DeviceTopicMap::findByStateTopic(const char *topic) {
    return findByStateTopic(topic, nullptr);
}

DeviceTopicEntry *DeviceTopicMap::findByStateTopic(const char *topic, uint8_t *topicEndpoint) {
    return findByMappedTopic(head, topic, topicEndpoint, true);
}

DeviceTopicEntry *DeviceTopicMap::findByAvailabilityTopic(const char *topic) {
    if (topic == nullptr || topic[0] == '\0') {
        return nullptr;
    }
    for (DeviceTopicEntry *entry = head; entry != nullptr; entry = entry->next) {
        if (!entry->used || deviceTopicEmpty(entry->availabilityTopic)) {
            continue;
        }
        if (mappedTopicEquals(entry->availabilityTopic, topic)) {
            return entry;
        }
    }
    return nullptr;
}

DeviceTopicEntry *DeviceTopicMap::upsert(
    const uint8_t ieee[8],
    const char *friendlyName,
    const char *stateTopic,
    const char *commandTopic,
    const char *availabilityTopic,
    uint8_t channelCount
) {
    DeviceTopicEntry *entry = findByIeee(ieee);
    uint8_t preservedFullControl = 0;
    uint8_t preservedDebug = 0;
    uint8_t preservedPublishToMqtt = 0;
    uint8_t preservedZigbeeProtocol = 0;
    uint8_t preservedZigbeeType = ZigbeeDeviceTypeUnknown;
    uint8_t preservedManufacturer = DeviceManufacturerUnknown;
    uint8_t preservedTransport = DeviceTransportZigbee;
    uint16_t preservedShortAddr = 0;
    if (entry != nullptr && entry->used) {
        preservedFullControl = entry->fullControl;
        preservedDebug = entry->debug;
        preservedPublishToMqtt = entry->publishToMqtt;
        preservedZigbeeProtocol = entry->zigbeeProtocol;
        preservedZigbeeType = entry->zigbeeType;
        preservedManufacturer = entry->manufacturer;
        preservedTransport = clampDeviceTransport(entry->transport);
        preservedShortAddr = entry->shortAddr;
    }
    if (entry == nullptr) {
        entry = allocateEntry();
    }
    if (entry == nullptr) {
        return nullptr;
    }

    memcpy(entry->ieee, ieee, 8);
    entry->used = 1;
    assignEntryStrings(entry, friendlyName, stateTopic, commandTopic, availabilityTopic);
    entry->channelCount = normalizeChannelCount(channelCount);
    entry->fullControl = preservedFullControl;
    entry->debug = preservedDebug;
    entry->publishToMqtt = preservedPublishToMqtt;
    entry->zigbeeProtocol = preservedZigbeeProtocol;
    entry->zigbeeType = preservedZigbeeType;
    entry->manufacturer = preservedManufacturer;
    entry->transport = preservedTransport;
    entry->shortAddr = preservedShortAddr;
    return entry;
}

DeviceTopicEntry *DeviceTopicMap::upsertFromEntry(const DeviceTopicEntry *source, bool keepExistingFullControl) {
    if (source == nullptr || !source->used) {
        return nullptr;
    }
    DeviceTopicEntry *entry = upsert(
        source->ieee,
        source->friendlyName,
        source->stateTopic,
        source->commandTopic,
        source->availabilityTopic,
        source->channelCount
    );
    if (entry == nullptr) {
        return nullptr;
    }
    if (!keepExistingFullControl || source->fullControl) {
        entry->fullControl = source->fullControl ? 1 : 0;
    }
    entry->debug = source->debug ? 1 : 0;
    entry->publishToMqtt = source->publishToMqtt ? 1 : 0;
    entry->zigbeeProtocol = source->zigbeeProtocol ? 1 : 0;
    if (source->zigbeeType != ZigbeeDeviceTypeUnknown) {
        entry->zigbeeType = source->zigbeeType;
    }
    if (source->manufacturer != DeviceManufacturerUnknown
        || entry->manufacturer == DeviceManufacturerUnknown) {
        entry->manufacturer = source->manufacturer;
    }
    entry->transport = clampDeviceTransport(source->transport);
    if (isUsableShortAddr(source->shortAddr)) {
        applyLastKnownShort(entry->ieee, source->shortAddr);
    }
    // Schema lives on the host only. Do not wipe it when the source has no fields
    // (e.g. SPI sync payloads never carry userDataFields).
    if (source->userDataFieldCount > 0) {
        if (!userDataCopyFieldDefs(
                &entry->userDataFields,
                &entry->userDataFieldCount,
                source->userDataFields,
                source->userDataFieldCount
            )) {
            userDataClearFieldDefs(&entry->userDataFields, &entry->userDataFieldCount);
        }
    } else if (!zigbeeDeviceTypeIsUserData(entry->zigbeeType)) {
        userDataClearFieldDefs(&entry->userDataFields, &entry->userDataFieldCount);
    }
    // Custom commands are host-only; SPI sync never carries them — keep existing.
    if (source->customCommandCount > 0) {
        if (!deviceCustomCommandCopy(
                &entry->customCommands,
                &entry->customCommandCount,
                source->customCommands,
                source->customCommandCount
            )) {
            deviceCustomCommandClear(&entry->customCommands, &entry->customCommandCount);
        }
    }
    // Reportable data is host-only; SPI sync never carries it — keep existing when absent.
    if (source->reportableDataCount > 0) {
        if (!reportableDataCopy(
                &entry->reportableData,
                &entry->reportableDataCount,
                source->reportableData,
                source->reportableDataCount
            )) {
            reportableDataClear(&entry->reportableData, &entry->reportableDataCount);
        }
    }
    return entry;
}

bool DeviceTopicMap::removeByIeee(const uint8_t ieee[8]) {
    DeviceTopicEntry *entry = findByIeee(ieee);
    if (entry == nullptr) {
        return false;
    }
    unlinkEntry(entry);
    freeEntry(entry);
    return true;
}

void DeviceTopicMap::clearAll() {
    while (head != nullptr) {
        DeviceTopicEntry *next = head->next;
        freeEntry(head);
        head = next;
    }
    head = nullptr;
}

void DeviceTopicMap::replaceFrom(const DeviceTopicMap *source) {
    clearAll();
    if (source == nullptr) {
        return;
    }
    for (const DeviceTopicEntry *sourceEntry = source->head; sourceEntry != nullptr; sourceEntry = sourceEntry->next) {
        if (!sourceEntry->used) {
            continue;
        }
        upsertFromEntry(sourceEntry, false);
    }
}

void DeviceTopicMap::copyFullControlFrom(const DeviceTopicMap *source) {
    if (source == nullptr) {
        return;
    }
    for (DeviceTopicEntry *entry = head; entry != nullptr; entry = entry->next) {
        if (!entry->used) {
            continue;
        }
        const DeviceTopicEntry *previous = source->findByIeee(entry->ieee);
        if (previous != nullptr) {
            entry->fullControl = previous->fullControl;
            entry->debug = previous->debug;
        }
    }
}

void DeviceTopicMap::copyZigbeeTypeFrom(const DeviceTopicMap *source) {
    if (source == nullptr) {
        return;
    }
    for (DeviceTopicEntry *entry = head; entry != nullptr; entry = entry->next) {
        if (!entry->used) {
            continue;
        }
        const DeviceTopicEntry *previous = source->findByIeee(entry->ieee);
        if (previous == nullptr) {
            continue;
        }
        entry->zigbeeType = mergeZigbeeDeviceType(entry->zigbeeType, previous->zigbeeType);
    }
}

void DeviceTopicMap::keepTransportEntriesFrom(const DeviceTopicMap *source, uint8_t transport) {
    if (source == nullptr) {
        return;
    }
    const uint8_t wantedTransport = clampDeviceTransport(transport);
    for (const DeviceTopicEntry *entry = source->head; entry != nullptr; entry = entry->next) {
        if (!entry->used || clampDeviceTransport(entry->transport) != wantedTransport) {
            continue;
        }
        upsertFromEntry(entry, false);
    }
}

int DeviceTopicMap::nextUsedIndex(int startIndex) const {
    if (startIndex < 0) {
        startIndex = 0;
    }
    int ordinal = 0;
    for (const DeviceTopicEntry *entry = head; entry != nullptr; entry = entry->next) {
        if (!entry->used) {
            continue;
        }
        if (ordinal >= startIndex) {
            return ordinal;
        }
        ordinal++;
    }
    return -1;
}

static const char *findJsonObjectEnd(const char *objectStart) {
    if (objectStart == nullptr || *objectStart != '{') {
        return nullptr;
    }
    int depth = 0;
    bool inString = false;
    bool escape = false;
    for (const char *cursor = objectStart; *cursor != '\0'; cursor++) {
        const char character = *cursor;
        if (inString) {
            if (escape) {
                escape = false;
                continue;
            }
            if (character == '\\') {
                escape = true;
                continue;
            }
            if (character == '"') {
                inString = false;
            }
            continue;
        }
        if (character == '"') {
            inString = true;
            continue;
        }
        if (character == '{' || character == '[') {
            depth++;
            continue;
        }
        if (character == '}' || character == ']') {
            depth--;
            if (depth == 0 && character == '}') {
                return cursor;
            }
        }
    }
    return nullptr;
}

void DeviceTopicMap::replaceFromJson(const String &json) {
    clearAll();
    const char *cursor = json.c_str();
    while (cursor != nullptr && *cursor != '\0') {
        const char *objectStart = strchr(cursor, '{');
        if (objectStart == nullptr) {
            break;
        }
        const char *objectEnd = findJsonObjectEnd(objectStart);
        if (objectEnd == nullptr) {
            break;
        }
        String object = String(objectStart).substring(0, (unsigned int)(objectEnd - objectStart + 1));
        String ieeeText;
        String friendlyName;
        String stateTopic;
        String commandTopic;
        String availability;
        extractJsonString(object.c_str(), "ieee", ieeeText);
        extractJsonString(object.c_str(), "name", friendlyName);
        if (friendlyName.length() == 0) {
            extractJsonString(object.c_str(), "friendlyName", friendlyName);
        }
        extractJsonString(object.c_str(), "state", stateTopic);
        extractJsonString(object.c_str(), "command", commandTopic);
        extractJsonString(object.c_str(), "availability", availability);
        int parsedChannels = DEVICE_CHANNEL_COUNT_DEFAULT;
        if (!extractJsonInt(object.c_str(), "channels", parsedChannels)) {
            parsedChannels = DEVICE_CHANNEL_COUNT_DEFAULT;
        }
        uint8_t ieee[8];
        if (ieeeText.length() > 0 && parseIeee(ieeeText.c_str(), ieee)) {
            DeviceTopicEntry *entry = upsert(
                ieee,
                friendlyName.c_str(),
                stateTopic.c_str(),
                commandTopic.c_str(),
                availability.c_str(),
                normalizeChannelCount(parsedChannels)
            );
            bool parsedFullControl = false;
            if (entry != nullptr && extractJsonBool(object.c_str(), "fullControl", parsedFullControl)) {
                entry->fullControl = parsedFullControl ? 1 : 0;
            }
            bool parsedDebug = false;
            if (entry != nullptr && extractJsonBool(object.c_str(), "debug", parsedDebug)) {
                entry->debug = parsedDebug ? 1 : 0;
            }
            const char *typeKey = strstr(object.c_str(), "\"type\":");
            if (entry != nullptr && typeKey != nullptr) {
                String typeText;
                if (extractJsonString(typeKey, "type", typeText)) {
                    entry->zigbeeType = zigbeeDeviceTypeFromJsonId(typeText.c_str());
                }
            }
            if (entry != nullptr) {
                String manufacturerText;
                if (extractJsonString(object.c_str(), "manufacturer", manufacturerText)) {
                    entry->manufacturer = deviceManufacturerFromJsonId(manufacturerText.c_str());
                } else {
                    entry->manufacturer = DeviceManufacturerUnknown;
                }
                String transportText;
                if (extractJsonString(object.c_str(), "transport", transportText)) {
                    entry->transport = deviceTransportFromJsonId(transportText.c_str());
                } else {
                    entry->transport = DeviceTransportZigbee;
                }
                if (clampDeviceTransport(entry->transport) == DeviceTransportMqtt) {
                    bool parsedZigbeeProtocol = false;
                    if (extractJsonBool(object.c_str(), "zigbeeProtocol", parsedZigbeeProtocol)) {
                        entry->zigbeeProtocol = parsedZigbeeProtocol ? 1 : 0;
                    } else {
                        entry->zigbeeProtocol = 0;
                    }
                } else {
                    entry->zigbeeProtocol = 0;
                    bool parsedPublishToMqtt = false;
                    if (extractJsonBool(object.c_str(), "publishToMqtt", parsedPublishToMqtt)) {
                        entry->publishToMqtt = parsedPublishToMqtt ? 1 : 0;
                    } else {
                        entry->publishToMqtt = 1;
                    }
                }
                int parsedNwk = 0;
                if (extractJsonInt(object.c_str(), "nwk", parsedNwk)
                    && parsedNwk > 0
                    && parsedNwk <= 0xFFFF) {
                    applyLastKnownShort(entry->ieee, (uint16_t)parsedNwk);
                } else {
                    String nwkText;
                    if (extractJsonString(object.c_str(), "nwk", nwkText) && nwkText.length() > 0) {
                        char *endPtr = nullptr;
                        const unsigned long parsedHex = strtoul(nwkText.c_str(), &endPtr, 0);
                        if (endPtr != nwkText.c_str()
                            && parsedHex > 0
                            && parsedHex <= 0xFFFFUL) {
                            applyLastKnownShort(entry->ieee, (uint16_t)parsedHex);
                        }
                    }
                }
                userDataClearFieldDefs(&entry->userDataFields, &entry->userDataFieldCount);
                if (zigbeeDeviceTypeIsUserData(entry->zigbeeType)) {
                    String fieldsSlice;
                    if (extractJsonKeyedSlice(object.c_str(), "userDataFields", '[', fieldsSlice)
                        && fieldsSlice.length() > 0) {
                        userDataAssignFieldsFromJson(
                            &entry->userDataFields,
                            &entry->userDataFieldCount,
                            fieldsSlice.c_str()
                        );
                    }
                }
                deviceCustomCommandClear(&entry->customCommands, &entry->customCommandCount);
                String customCommandsSlice;
                if (extractJsonKeyedSlice(object.c_str(), "customCommands", '[', customCommandsSlice)
                    && customCommandsSlice.length() > 0) {
                    bool customValid = true;
                    deviceCustomCommandAssignFromJson(
                        &entry->customCommands,
                        &entry->customCommandCount,
                        customCommandsSlice.c_str(),
                        &customValid
                    );
                    if (!customValid) {
                        deviceCustomCommandClear(&entry->customCommands, &entry->customCommandCount);
                    }
                }
                reportableDataClear(&entry->reportableData, &entry->reportableDataCount);
                String reportableSlice;
                if (extractJsonKeyedSlice(object.c_str(), "reportableData", '[', reportableSlice)
                    && reportableSlice.length() > 0) {
                    reportableDataAssignFromJson(
                        &entry->reportableData,
                        &entry->reportableDataCount,
                        reportableSlice.c_str()
                    );
                }
            }
        }
        cursor = objectEnd + 1;
    }
}

bool DeviceTopicMap::loadFromFile(const char *path) {
    if (path == nullptr) {
        return false;
    }
    File file = LittleFS.open(path, "r");
    if (!file) {
        return false;
    }
    String json = file.readString();
    file.close();
    json.trim();
    if (json.length() == 0 || json.charAt(0) != '[') {
        return false;
    }
    replaceFromJson(json);
    return true;
}

bool DeviceTopicMap::saveToFile(const char *path) {
    if (path == nullptr) {
        return false;
    }
    File file = LittleFS.open(path, "w");
    if (!file) {
        return false;
    }
    const String json = listStoreJson();
    const size_t written = file.print(json);
    file.close();
    return written == json.length();
}

size_t DeviceTopicMap::packSyncPayload(
    uint8_t *out,
    size_t outMax,
    uint8_t flags,
    const DeviceTopicEntry *entry
) {
    if (out == nullptr || outMax < 1) {
        return 0;
    }
    const bool hasEntry = entry != nullptr
        && ((flags & SPI_DEVICE_SYNC_ENTRY) != 0 || (flags & SPI_DEVICE_SYNC_DELETE) != 0);
    const bool includeNwk = hasEntry && outMax >= kDeviceSyncEntryLenWithNwk;
    const size_t length = !hasEntry
        ? 1
        : (includeNwk ? kDeviceSyncEntryLenWithNwk : SPI_DEVICE_SYNC_ENTRY_LEN);
    if (outMax < length) {
        return 0;
    }
    memset(out, 0, length);
    out[0] = flags;
    if (hasEntry) {
        memcpy(out + 1, entry->ieee, 8);
        strncpy((char *)out + 9, deviceTopicCStr(entry->friendlyName), SPI_DEVICE_SYNC_NAME_LEN - 1);
        strncpy((char *)out + 33, deviceTopicCStr(entry->stateTopic), SPI_DEVICE_SYNC_TOPIC_LEN - 1);
        strncpy((char *)out + 97, deviceTopicCStr(entry->commandTopic), SPI_DEVICE_SYNC_TOPIC_LEN - 1);
        strncpy((char *)out + 161, deviceTopicCStr(entry->availabilityTopic), SPI_DEVICE_SYNC_TOPIC_LEN - 1);
        out[SPI_DEVICE_SYNC_ENTRY_LEN_NO_CHANNELS] = entry->channelCount;
        // Bit0 = fullControl, bit1 = debug (legacy hosts ignore bit1).
        out[SPI_DEVICE_SYNC_ENTRY_LEN_WITH_CHANNELS] =
            (entry->fullControl ? 0x01 : 0x00) | (entry->debug ? 0x02 : 0x00);
        out[SPI_DEVICE_SYNC_ENTRY_LEN_WITH_FULL_CONTROL] = entry->zigbeeType;
        if (includeNwk) {
            const uint16_t shortAddr = isUsableShortAddr(entry->shortAddr) ? entry->shortAddr : 0;
            out[SPI_DEVICE_SYNC_ENTRY_LEN] = (uint8_t)(shortAddr & 0xFF);
            out[SPI_DEVICE_SYNC_ENTRY_LEN + 1] = (uint8_t)((shortAddr >> 8) & 0xFF);
        }
    }
    return length;
}

bool DeviceTopicMap::unpackSyncPayload(
    const uint8_t *in,
    uint16_t length,
    uint8_t *flags,
    DeviceTopicEntry *entry
) {
    if (in == nullptr || flags == nullptr || length < 1) {
        return false;
    }
    *flags = in[0];
    if (entry != nullptr) {
        // Callers pass a fresh/zeroed entry; do not free() garbage stack pointers.
        memset(entry, 0, sizeof(DeviceTopicEntry));
    }
    if ((*flags & (SPI_DEVICE_SYNC_ENTRY | SPI_DEVICE_SYNC_DELETE)) == 0) {
        return true;
    }
    if (length < SPI_DEVICE_SYNC_ENTRY_LEN_NO_CHANNELS || entry == nullptr) {
        return false;
    }
    memcpy(entry->ieee, in + 1, 8);
    entry->friendlyName = duplicateBoundedString((const char *)in + 9, SPI_DEVICE_SYNC_NAME_LEN);
    entry->stateTopic = duplicateBoundedString((const char *)in + 33, SPI_DEVICE_SYNC_TOPIC_LEN);
    entry->commandTopic = duplicateBoundedString((const char *)in + 97, SPI_DEVICE_SYNC_TOPIC_LEN);
    entry->availabilityTopic = duplicateBoundedString((const char *)in + 161, SPI_DEVICE_SYNC_TOPIC_LEN);
    if (length >= SPI_DEVICE_SYNC_ENTRY_LEN_WITH_CHANNELS) {
        entry->channelCount = normalizeChannelCount(in[SPI_DEVICE_SYNC_ENTRY_LEN_NO_CHANNELS]);
    } else {
        entry->channelCount = DEVICE_CHANNEL_COUNT_DEFAULT;
    }
    if (length >= SPI_DEVICE_SYNC_ENTRY_LEN_WITH_FULL_CONTROL) {
        const uint8_t flagsByte = in[SPI_DEVICE_SYNC_ENTRY_LEN_WITH_CHANNELS];
        entry->fullControl = (flagsByte & 0x01) != 0 ? 1 : 0;
        entry->debug = (flagsByte & 0x02) != 0 ? 1 : 0;
    }
    if (length >= SPI_DEVICE_SYNC_ENTRY_LEN) {
        entry->zigbeeType = in[SPI_DEVICE_SYNC_ENTRY_LEN_WITH_FULL_CONTROL];
    }
    if (length >= kDeviceSyncEntryLenWithNwk) {
        entry->shortAddr = (uint16_t)in[SPI_DEVICE_SYNC_ENTRY_LEN]
            | ((uint16_t)in[SPI_DEVICE_SYNC_ENTRY_LEN + 1] << 8);
        if (!isUsableShortAddr(entry->shortAddr)) {
            entry->shortAddr = 0;
        }
    } else {
        entry->shortAddr = 0;
    }
    entry->transport = DeviceTransportZigbee;
    entry->used = 1;
    return true;
}

String DeviceTopicMap::formatIeee(const uint8_t ieee[8]) {
    char buffer[24];
    snprintf(
        buffer,
        sizeof(buffer),
        "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
        ieee[7],
        ieee[6],
        ieee[5],
        ieee[4],
        ieee[3],
        ieee[2],
        ieee[1],
        ieee[0]
    );
    return String(buffer);
}

static int hexNibble(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

bool DeviceTopicMap::parseIeee(const char *text, uint8_t ieee[8]) {
    if (text == nullptr) {
        return false;
    }
    uint8_t msbFirst[8];
    int filled = 0;
    int highNibble = -1;
    for (const char *cursor = text; *cursor != '\0'; cursor++) {
        if (*cursor == ':' || *cursor == '-' || *cursor == ' ') {
            continue;
        }
        int nibble = hexNibble(*cursor);
        if (nibble < 0) {
            return false;
        }
        if (highNibble < 0) {
            highNibble = nibble;
        } else {
            if (filled >= 8) {
                return false;
            }
            msbFirst[filled++] = (uint8_t)((highNibble << 4) | nibble);
            highNibble = -1;
        }
    }
    if (filled != 8 || highNibble >= 0) {
        return false;
    }
    for (int i = 0; i < 8; i++) {
        ieee[i] = msbFirst[7 - i];
    }
    return true;
}

String DeviceTopicMap::listJson() {
    return listJson(nullptr, nullptr);
}

static void appendDeviceListEntryJson(
    DeviceTopicMap *deviceMap,
    String &json,
    const DeviceTopicEntry *deviceEntry,
    DeviceTopicMap::OnlineFn isOnline,
    DeviceTopicMap::LastRssiFn lastRssi,
    DeviceTopicMap::ListTelemetryFn telemetry
) {
    if (deviceEntry == nullptr) {
        json += "null";
        return;
    }
    json += "{\"ieee\":\"";
    json += deviceMap->formatIeee(deviceEntry->ieee);
    json += "\",\"name\":\"";
    appendJsonEscaped(json, deviceEntry->friendlyName, jsonEscapeCap(deviceEntry->friendlyName));
    json += "\",\"state\":\"";
    appendJsonEscaped(json, deviceEntry->stateTopic, jsonEscapeCap(deviceEntry->stateTopic));
    json += "\",\"command\":\"";
    appendJsonEscaped(json, deviceEntry->commandTopic, jsonEscapeCap(deviceEntry->commandTopic));
    json += "\",\"availability\":\"";
    appendJsonEscaped(json, deviceEntry->availabilityTopic, jsonEscapeCap(deviceEntry->availabilityTopic));
    json += "\",\"channels\":";
    json += String(deviceEntry->channelCount);
    json += ",\"nwk\":";
    json += String((unsigned)deviceEntry->shortAddr);
    json += ",\"fullControl\":";
    json += deviceEntry->fullControl ? "true" : "false";
    json += ",\"debug\":";
    json += deviceEntry->debug ? "true" : "false";
    json += ",\"type\":\"";
    json += zigbeeDeviceTypeJsonId(deviceEntry->zigbeeType);
    json += "\",\"manufacturer\":\"";
    json += deviceManufacturerJsonId(deviceEntry->manufacturer);
    json += "\",\"configurable\":";
    json += deviceHasConfigurableAttrs(
                deviceEntry->transport,
                deviceEntry->manufacturer,
                deviceEntry->zigbeeType
            )
        ? "true"
        : "false";
    json += ",\"transport\":\"";
    json += deviceTransportJsonId(deviceEntry->transport);
    json += "\"";
    if (clampDeviceTransport(deviceEntry->transport) == DeviceTransportMqtt) {
        json += ",\"zigbeeProtocol\":";
        json += deviceEntry->zigbeeProtocol ? "true" : "false";
    } else {
        json += ",\"publishToMqtt\":";
        json += deviceEntry->publishToMqtt ? "true" : "false";
    }
    if (zigbeeDeviceTypeIsUserData(deviceEntry->zigbeeType)) {
        json += ",\"userDataFields\":";
        json += userDataFieldsToJson(deviceEntry->userDataFields, deviceEntry->userDataFieldCount);
    }
    json += ",\"customCommands\":";
    json += deviceCustomCommandsToJson(deviceEntry->customCommands, deviceEntry->customCommandCount);
    json += ",\"reportableData\":";
    json += reportableDataToJson(deviceEntry->reportableData, deviceEntry->reportableDataCount);
    if (isOnline != nullptr) {
        json += ",\"online\":";
        json += isOnline(deviceEntry->ieee) ? "true" : "false";
    }
    if (lastRssi != nullptr) {
        int8_t rssiDbm = 0;
        if (lastRssi(deviceEntry->ieee, &rssiDbm)) {
            json += ",\"rssi\":";
            json += String((int)rssiDbm);
        }
    }
    if (telemetry != nullptr) {
        telemetry(deviceEntry->ieee, json);
    }
    json += "}";
}

String DeviceTopicMap::listJson(OnlineFn isOnline) {
    return listJson(isOnline, nullptr);
}

String DeviceTopicMap::listJson(OnlineFn isOnline, LastRssiFn lastRssi) {
    return listJson(isOnline, lastRssi, nullptr);
}

String DeviceTopicMap::listJson(OnlineFn isOnline, LastRssiFn lastRssi, ListTelemetryFn telemetry) {
    String json = "[";
    bool first = true;
    size_t usedCount = 0;
    for (DeviceTopicEntry *deviceEntry = head; deviceEntry != nullptr; deviceEntry = deviceEntry->next) {
        if (deviceEntry->used) {
            usedCount++;
        }
    }
    json.reserve(usedCount * 320 + 2);
    for (DeviceTopicEntry *deviceEntry = head; deviceEntry != nullptr; deviceEntry = deviceEntry->next) {
        if (!deviceEntry->used) {
            continue;
        }
        if (!first) {
            json += ',';
        }
        first = false;
        appendDeviceListEntryJson(this, json, deviceEntry, isOnline, lastRssi, telemetry);
    }
    json += "]";
    return json;
}

String DeviceTopicMap::entryJson(
    const uint8_t ieee[8],
    OnlineFn isOnline,
    LastRssiFn lastRssi,
    ListTelemetryFn telemetry
) {
    const DeviceTopicEntry *deviceEntry = findByIeee(ieee);
    if (deviceEntry == nullptr || !deviceEntry->used) {
        return String();
    }
    String json;
    appendDeviceListEntryJson(this, json, deviceEntry, isOnline, lastRssi, telemetry);
    return json;
}

String DeviceTopicMap::listStoreJson() {
    return listJson(nullptr);
}
