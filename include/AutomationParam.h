#pragma once

#include "ZigbeeCluster.h"
#include "ZigbeeDeviceType.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

constexpr size_t AUTOMATION_PARAM_KEY_MAX = 24;

inline bool automationParamIsCustom(const char *param) {
    return param != nullptr && strcmp(param, "custom") == 0;
}

inline bool automationParamIsKnownKey(const char *param) {
    if (param == nullptr || param[0] == '\0' || automationParamIsCustom(param)) {
        return false;
    }
    return strcmp(param, "status") == 0
        || strcmp(param, "position") == 0
        || strcmp(param, "battery") == 0
        || strcmp(param, "sensorValue") == 0
        || strcmp(param, "lightLevel") == 0;
}

// Device action command ids (and free-form / legacy payloads use these or payload-only).
inline bool automationParamIsActionCommand(const char *param) {
    if (param == nullptr || param[0] == '\0' || automationParamIsCustom(param)) {
        return false;
    }
    return strcmp(param, "ON") == 0
        || strcmp(param, "OFF") == 0
        || strcmp(param, "TOGGLE") == 0
        || strcmp(param, "OPEN") == 0
        || strcmp(param, "CLOSE") == 0
        || strcmp(param, "STOP") == 0
        || strcmp(param, "MOVETO") == 0
        || strcmp(param, "LIGHT_LEVEL") == 0
        || automationParamIsKnownKey(param);
}

inline bool automationActionPayloadOk(const char *payload) {
    if (payload == nullptr) {
        return false;
    }
    while (*payload == ' ' || *payload == '\t') {
        payload++;
    }
    return payload[0] != '\0';
}

struct AutomationParamBinding {
    uint16_t cluster;
    uint16_t attrId;
    bool hasCluster;
    bool hasAttrId;
};

inline bool automationParamResolve(
    const char *param,
    ZigbeeDeviceType deviceType,
    AutomationParamBinding &out
) {
    memset(&out, 0, sizeof(out));
    if (!automationParamIsKnownKey(param)) {
        return false;
    }
    if (strcmp(param, "battery") == 0) {
        out.hasCluster = true;
        out.cluster = kZigbeeClusterPowerConfig;
        out.hasAttrId = true;
        out.attrId = kZigbeeAttrBatteryPercentageRemaining;
        return true;
    }
    if (strcmp(param, "position") == 0) {
        out.hasCluster = true;
        out.cluster = kZigbeeClusterWindowCovering;
        out.hasAttrId = true;
        out.attrId = kZigbeeAttrCurrentPositionLiftPercentage;
        return true;
    }
    if (strcmp(param, "lightLevel") == 0) {
        out.hasCluster = true;
        out.cluster = kZigbeeClusterLevelControl;
        out.hasAttrId = true;
        out.attrId = 0x0000;
        return true;
    }
    if (strcmp(param, "sensorValue") == 0) {
        out.hasAttrId = true;
        out.attrId = 0x0000;
        if (deviceType == ZigbeeDeviceTypeUnknown) {
            out.hasCluster = false;
            return true;
        }
        const ZigbeeDeviceTypeInfo *info = zigbeeDeviceTypeInfo((uint8_t)deviceType);
        if (info != nullptr && info->clusterId != kZigbeeDeviceTypeNoCluster) {
            out.hasCluster = true;
            out.cluster = info->clusterId;
        }
        return true;
    }
    if (strcmp(param, "status") == 0) {
        out.hasCluster = true;
        out.hasAttrId = true;
        if (deviceType == ZigbeeDeviceTypeWindowCovering) {
            out.cluster = kZigbeeClusterWindowCovering;
            out.attrId = kZigbeeAttrWindowCoveringMoveStatus;
            return true;
        }
        if (deviceType == ZigbeeDeviceTypeIasZone) {
            out.cluster = kZigbeeClusterIasZone;
            out.attrId = kZigbeeAttrIasZoneStatus;
            return true;
        }
        out.cluster = kZigbeeClusterOnOff;
        out.attrId = 0x0000;
        return true;
    }
    return false;
}

inline bool automationParamKeyAllowedForType(const char *param, ZigbeeDeviceType deviceType) {
    if (automationParamIsCustom(param)) {
        return true;
    }
    if (!automationParamIsKnownKey(param)) {
        return false;
    }
    if (strcmp(param, "battery") == 0) {
        return true;
    }
    if (strcmp(param, "position") == 0) {
        return deviceType == ZigbeeDeviceTypeWindowCovering;
    }
    if (strcmp(param, "lightLevel") == 0) {
        return deviceType == ZigbeeDeviceTypeOnOff || deviceType == ZigbeeDeviceTypeUnknown;
    }
    if (strcmp(param, "sensorValue") == 0) {
        return deviceType != ZigbeeDeviceTypeOnOff
            && deviceType != ZigbeeDeviceTypeWindowCovering;
    }
    if (strcmp(param, "status") == 0) {
        return true;
    }
    return false;
}

inline uint16_t automationParseHexOrDecToken(const char *text) {
    if (text == nullptr) {
        return 0;
    }
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    return (uint16_t)strtoul(text, nullptr, 0);
}

inline uint16_t automationParseAttrIdFromState(const char *state) {
    if (state == nullptr) {
        return 0;
    }
    const char *mark = strstr(state, "attr=0x");
    if (mark != nullptr) {
        return (uint16_t)strtoul(mark + 7, nullptr, 16);
    }
    mark = strstr(state, "attr=");
    if (mark != nullptr) {
        return (uint16_t)strtoul(mark + 5, nullptr, 0);
    }
    return 0;
}

// Old UI stored typed hex digits as decimal (e.g. "500" for IAS 0x0500 → stored 500).
inline bool automationClusterIdMatches(uint16_t listenerCluster, uint16_t eventCluster) {
    if (listenerCluster == eventCluster) {
        return true;
    }
    char hexDigits[12];
    snprintf(hexDigits, sizeof(hexDigits), "%x", (unsigned)eventCluster);
    char *end = nullptr;
    const unsigned long asDecimalDigits = strtoul(hexDigits, &end, 10);
    return end != hexDigits && *end == '\0' && asDecimalDigits == (unsigned long)listenerCluster;
}

inline bool automationAttrIdMatches(uint16_t listenerAttrId, uint16_t eventAttrId) {
    if (listenerAttrId == eventAttrId) {
        return true;
    }
    char hexDigits[12];
    snprintf(hexDigits, sizeof(hexDigits), "%x", (unsigned)eventAttrId);
    char *end = nullptr;
    const unsigned long asDecimalDigits = strtoul(hexDigits, &end, 10);
    return end != hexDigits && *end == '\0' && asDecimalDigits == (unsigned long)listenerAttrId;
}

// Interpreted MQTT/status strings omit cl=/attr=; map them back for CUSTOM listen matching.
inline uint16_t automationInferClusterFromInterpretedState(const char *state) {
    if (state == nullptr || state[0] == '\0') {
        return 0;
    }
    if (strcmp(state, "LEAK") == 0 || strcmp(state, "DRY") == 0) {
        return kZigbeeClusterIasZone;
    }
    if (strcmp(state, "ON") == 0 || strcmp(state, "OFF") == 0) {
        return kZigbeeClusterOnOff;
    }
    if (strcmp(state, "UP") == 0 || strcmp(state, "DOWN") == 0 || strcmp(state, "STOPPED") == 0) {
        return kZigbeeClusterWindowCovering;
    }
    return 0;
}

inline uint16_t automationInferAttrIdFromInterpretedState(const char *state) {
    if (state == nullptr || state[0] == '\0') {
        return 0;
    }
    if (strcmp(state, "LEAK") == 0 || strcmp(state, "DRY") == 0) {
        return kZigbeeAttrIasZoneStatus;
    }
    return 0;
}

inline bool automationParseNumericVal(const char *text, double &outValue) {
    if (text == nullptr || text[0] == '\0') {
        return false;
    }
    const char *cursor = text;
    while (*cursor != '\0' && !isdigit((unsigned char)*cursor) && *cursor != '-' && *cursor != '+') {
        cursor++;
    }
    if (*cursor == '\0') {
        return false;
    }
    char *end = nullptr;
    const double parsed = strtod(cursor, &end);
    if (end == cursor) {
        return false;
    }
    outValue = parsed;
    return true;
}

inline bool automationStateMatchesParam(const char *param, uint16_t clusterOr0, const char *newState) {
    if (param == nullptr || param[0] == '\0') {
        return true;
    }
    if (automationParamIsCustom(param)) {
        return true;
    }
    if (newState == nullptr) {
        return false;
    }
    if (strcmp(param, "battery") == 0) {
        if (clusterOr0 == kZigbeeClusterPowerConfig) {
            return true;
        }
        if (strncmp(newState, "BATTERY", 7) == 0) {
            return true;
        }
        // Plain percent number (preferred comparable form).
        char *batteryEnd = nullptr;
        strtod(newState, &batteryEnd);
        return batteryEnd != newState && *batteryEnd == '\0';
    }
    if (strcmp(param, "position") == 0) {
        if (clusterOr0 != 0 && clusterOr0 != kZigbeeClusterWindowCovering) {
            return false;
        }
        if (strchr(newState, '%') != nullptr) {
            return true;
        }
        double unused = 0;
        return automationParseNumericVal(newState, unused)
            && strcmp(newState, "ON") != 0
            && strcmp(newState, "OFF") != 0;
    }
    if (strcmp(param, "lightLevel") == 0) {
        if (clusterOr0 != 0 && clusterOr0 != kZigbeeClusterLevelControl) {
            return false;
        }
        double unused = 0;
        return automationParseNumericVal(newState, unused)
            && strcmp(newState, "ON") != 0
            && strcmp(newState, "OFF") != 0;
    }
    if (strcmp(param, "sensorValue") == 0) {
        if (strncmp(newState, "BATTERY", 7) == 0) {
            return false;
        }
        if (strcmp(newState, "ON") == 0 || strcmp(newState, "OFF") == 0) {
            return false;
        }
        return true;
    }
    if (strcmp(param, "status") == 0) {
        if (strncmp(newState, "BATTERY", 7) == 0) {
            return false;
        }
        if (strchr(newState, '%') != nullptr
            && strstr(newState, "UP") == nullptr
            && strstr(newState, "DOWN") == nullptr
            && strstr(newState, "STOPPED") == nullptr) {
            return false;
        }
        // Reject plain numbers (battery / position / level) — status is discrete text.
        char *statusEnd = nullptr;
        strtod(newState, &statusEnd);
        if (statusEnd != newState && *statusEnd == '\0') {
            return false;
        }
        return true;
    }
    return true;
}

inline bool automationBuildCustomPayload(
    uint16_t cluster,
    uint16_t command,
    const char *valueText,
    char *out,
    size_t outLen
) {
    if (out == nullptr || outLen < 24) {
        return false;
    }
    if (valueText != nullptr && valueText[0] != '\0') {
        snprintf(
            out,
            outLen,
            "cl=0x%04X,cmd=0x%02X,val=%s",
            (unsigned)cluster,
            (unsigned)(command & 0xFF),
            valueText
        );
    } else {
        snprintf(
            out,
            outLen,
            "cl=0x%04X,cmd=0x%02X",
            (unsigned)cluster,
            (unsigned)(command & 0xFF)
        );
    }
    return true;
}
