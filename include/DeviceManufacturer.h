#pragma once

#include <ctype.h>
#include <stdint.h>
#include <string.h>

enum DeviceManufacturer : uint8_t {
    DeviceManufacturerUnknown = 0,
    DeviceManufacturerTuya = 1,
    DeviceManufacturerAqara = 2,
    DeviceManufacturerPhilipsHue = 3,
    DeviceManufacturerIkea = 4,
    DeviceManufacturerSonoff = 5,
    DeviceManufacturerLedvance = 6,
    DeviceManufacturerInnr = 7,
    DeviceManufacturerGledopto = 8,
    DeviceManufacturerSengled = 9,
    DeviceManufacturerThirdReality = 10,
    DeviceManufacturerHeiman = 11,
    DeviceManufacturerLegrand = 12,
    DeviceManufacturerSchneider = 13,
    DeviceManufacturerNamron = 14,
    DeviceManufacturerDevelco = 15,
    DeviceManufacturerZbeacon = 16
};

inline const char *deviceManufacturerJsonId(uint8_t manufacturer) {
    switch (manufacturer) {
        case DeviceManufacturerTuya:
            return "tuya";
        case DeviceManufacturerAqara:
            return "aqara";
        case DeviceManufacturerPhilipsHue:
            return "philipsHue";
        case DeviceManufacturerIkea:
            return "ikea";
        case DeviceManufacturerSonoff:
            return "sonoff";
        case DeviceManufacturerLedvance:
            return "ledvance";
        case DeviceManufacturerInnr:
            return "innr";
        case DeviceManufacturerGledopto:
            return "gledopto";
        case DeviceManufacturerSengled:
            return "sengled";
        case DeviceManufacturerThirdReality:
            return "thirdReality";
        case DeviceManufacturerHeiman:
            return "heiman";
        case DeviceManufacturerLegrand:
            return "legrand";
        case DeviceManufacturerSchneider:
            return "schneider";
        case DeviceManufacturerNamron:
            return "namron";
        case DeviceManufacturerDevelco:
            return "develco";
        case DeviceManufacturerZbeacon:
            return "zbeacon";
        default:
            return "unknown";
    }
}

inline const char *deviceManufacturerLabel(uint8_t manufacturer) {
    switch (manufacturer) {
        case DeviceManufacturerTuya:
            return "Tuya";
        case DeviceManufacturerAqara:
            return "Aqara";
        case DeviceManufacturerPhilipsHue:
            return "Philips Hue";
        case DeviceManufacturerIkea:
            return "IKEA";
        case DeviceManufacturerSonoff:
            return "Sonoff";
        case DeviceManufacturerLedvance:
            return "LEDVANCE / OSRAM";
        case DeviceManufacturerInnr:
            return "Innr";
        case DeviceManufacturerGledopto:
            return "Gledopto";
        case DeviceManufacturerSengled:
            return "Sengled";
        case DeviceManufacturerThirdReality:
            return "Third Reality";
        case DeviceManufacturerHeiman:
            return "Heiman";
        case DeviceManufacturerLegrand:
            return "Legrand";
        case DeviceManufacturerSchneider:
            return "Schneider";
        case DeviceManufacturerNamron:
            return "Namron";
        case DeviceManufacturerDevelco:
            return "Develco";
        case DeviceManufacturerZbeacon:
            return "Zbeacon";
        default:
            return "Unknown";
    }
}

inline DeviceManufacturer deviceManufacturerFromJsonId(const char *jsonId) {
    if (jsonId == nullptr) {
        return DeviceManufacturerUnknown;
    }
    if (strcmp(jsonId, "tuya") == 0) {
        return DeviceManufacturerTuya;
    }
    if (strcmp(jsonId, "aqara") == 0) {
        return DeviceManufacturerAqara;
    }
    if (strcmp(jsonId, "philipsHue") == 0) {
        return DeviceManufacturerPhilipsHue;
    }
    if (strcmp(jsonId, "ikea") == 0) {
        return DeviceManufacturerIkea;
    }
    if (strcmp(jsonId, "sonoff") == 0) {
        return DeviceManufacturerSonoff;
    }
    if (strcmp(jsonId, "ledvance") == 0) {
        return DeviceManufacturerLedvance;
    }
    if (strcmp(jsonId, "innr") == 0) {
        return DeviceManufacturerInnr;
    }
    if (strcmp(jsonId, "gledopto") == 0) {
        return DeviceManufacturerGledopto;
    }
    if (strcmp(jsonId, "sengled") == 0) {
        return DeviceManufacturerSengled;
    }
    if (strcmp(jsonId, "thirdReality") == 0) {
        return DeviceManufacturerThirdReality;
    }
    if (strcmp(jsonId, "heiman") == 0) {
        return DeviceManufacturerHeiman;
    }
    if (strcmp(jsonId, "legrand") == 0) {
        return DeviceManufacturerLegrand;
    }
    if (strcmp(jsonId, "schneider") == 0) {
        return DeviceManufacturerSchneider;
    }
    if (strcmp(jsonId, "namron") == 0) {
        return DeviceManufacturerNamron;
    }
    if (strcmp(jsonId, "develco") == 0) {
        return DeviceManufacturerDevelco;
    }
    if (strcmp(jsonId, "zbeacon") == 0) {
        return DeviceManufacturerZbeacon;
    }
    return DeviceManufacturerUnknown;
}

inline bool deviceManufacturerStringContainsFold(
    const char *haystack,
    const char *needle
) {
    if (haystack == nullptr || needle == nullptr || needle[0] == '\0') {
        return false;
    }
    const size_t needleLength = strlen(needle);
    for (const char *cursor = haystack; *cursor != '\0'; cursor++) {
        size_t index = 0;
        while (index < needleLength) {
            const char left = cursor[index];
            const char right = needle[index];
            if (left == '\0') {
                break;
            }
            if (tolower((unsigned char)left) != tolower((unsigned char)right)) {
                break;
            }
            index++;
        }
        if (index == needleLength) {
            return true;
        }
    }
    return false;
}

inline DeviceManufacturer deviceManufacturerFromJoinString(const char *manufacturerName) {
    if (manufacturerName == nullptr || manufacturerName[0] == '\0') {
        return DeviceManufacturerUnknown;
    }
    if (deviceManufacturerStringContainsFold(manufacturerName, "_TZ")
        || deviceManufacturerStringContainsFold(manufacturerName, "tuya")) {
        return DeviceManufacturerTuya;
    }
    if (deviceManufacturerStringContainsFold(manufacturerName, "lumi")
        || deviceManufacturerStringContainsFold(manufacturerName, "aqara")
        || deviceManufacturerStringContainsFold(manufacturerName, "xiaomi")) {
        return DeviceManufacturerAqara;
    }
    if (deviceManufacturerStringContainsFold(manufacturerName, "philips")
        || deviceManufacturerStringContainsFold(manufacturerName, "signify")) {
        return DeviceManufacturerPhilipsHue;
    }
    if (deviceManufacturerStringContainsFold(manufacturerName, "ikea")) {
        return DeviceManufacturerIkea;
    }
    if (deviceManufacturerStringContainsFold(manufacturerName, "sonoff")
        || deviceManufacturerStringContainsFold(manufacturerName, "ewelink")) {
        return DeviceManufacturerSonoff;
    }
    if (deviceManufacturerStringContainsFold(manufacturerName, "ledvance")
        || deviceManufacturerStringContainsFold(manufacturerName, "osram")) {
        return DeviceManufacturerLedvance;
    }
    if (deviceManufacturerStringContainsFold(manufacturerName, "innr")) {
        return DeviceManufacturerInnr;
    }
    if (deviceManufacturerStringContainsFold(manufacturerName, "gledopto")) {
        return DeviceManufacturerGledopto;
    }
    if (deviceManufacturerStringContainsFold(manufacturerName, "sengled")) {
        return DeviceManufacturerSengled;
    }
    if (deviceManufacturerStringContainsFold(manufacturerName, "third reality")
        || deviceManufacturerStringContainsFold(manufacturerName, "thirdreality")) {
        return DeviceManufacturerThirdReality;
    }
    if (deviceManufacturerStringContainsFold(manufacturerName, "heiman")) {
        return DeviceManufacturerHeiman;
    }
    if (deviceManufacturerStringContainsFold(manufacturerName, "legrand")
        || deviceManufacturerStringContainsFold(manufacturerName, "netatmo")) {
        return DeviceManufacturerLegrand;
    }
    if (deviceManufacturerStringContainsFold(manufacturerName, "schneider")) {
        return DeviceManufacturerSchneider;
    }
    if (deviceManufacturerStringContainsFold(manufacturerName, "namron")) {
        return DeviceManufacturerNamron;
    }
    if (deviceManufacturerStringContainsFold(manufacturerName, "develco")) {
        return DeviceManufacturerDevelco;
    }
    if (deviceManufacturerStringContainsFold(manufacturerName, "zbeacon")) {
        return DeviceManufacturerZbeacon;
    }
    return DeviceManufacturerUnknown;
}
