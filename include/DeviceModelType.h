#pragma once

#include "DeviceManufacturer.h"
#include "ZigbeeDeviceType.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>

// Model/manufacturer identity wins over equal-rank cluster guesses at creation.
// Returns ZigbeeDeviceTypeUnknown when identity is incomplete or unknown.

inline bool deviceModelStringStartsWithFold(const char *model, const char *prefix) {
    if (model == nullptr || prefix == nullptr || prefix[0] == '\0') {
        return false;
    }
    for (size_t index = 0; prefix[index] != '\0'; index++) {
        const char left = model[index];
        const char right = prefix[index];
        if (left == '\0') {
            return false;
        }
        if (tolower((unsigned char)left) != tolower((unsigned char)right)) {
            return false;
        }
    }
    return true;
}

inline uint8_t zigbeeDeviceTypeFromIdentity(const char *manufacturer, const char *model) {
    if (manufacturer == nullptr || manufacturer[0] == '\0'
        || model == nullptr || model[0] == '\0') {
        return ZigbeeDeviceTypeUnknown;
    }
    const DeviceManufacturer brand = deviceManufacturerFromJoinString(manufacturer);
    if (brand == DeviceManufacturerZbeacon) {
        // Zbeacon climate / flower sensors (TH01, TS0201, TS0202, …): temperature is the
        // primary type; humidity + battery are seeded via reportableData for this brand.
        if (deviceModelStringStartsWithFold(model, "TH")
            || deviceModelStringStartsWithFold(model, "TS02")
            || deviceModelStringStartsWithFold(model, "ZY-")
            || deviceManufacturerStringContainsFold(model, "temp")
            || deviceManufacturerStringContainsFold(model, "humid")) {
            return zigbeeDeviceTypeFromCluster(kZigbeeClusterTemperature);
        }
        // Any other non-empty Zbeacon model still uses the brand climate seed.
        return zigbeeDeviceTypeFromCluster(kZigbeeClusterTemperature);
    }
    return ZigbeeDeviceTypeUnknown;
}
