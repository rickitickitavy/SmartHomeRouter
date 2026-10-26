#pragma once

#include "DeviceManufacturer.h"
#include "GlobalSettings.h"
#include "ZigbeeDeviceType.h"
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum DeviceConfigValueType : uint8_t {
    DeviceConfigValueBool = 0,
    DeviceConfigValueEnum = 1,
    DeviceConfigValueUInt8 = 2,
    DeviceConfigValueUInt16 = 3,
    DeviceConfigValueInt8 = 4,
    DeviceConfigValueInt16 = 5
};

struct DeviceConfigEnumChoice {
    const char *label;
    uint32_t value;
};

// Flash-resident catalog row. modelPrefix nullptr = any model for manufacturer+type.
struct DeviceConfigAttr {
    uint8_t manufacturer;
    uint8_t zigbeeType;
    const char *modelPrefix;
    const char *id;
    const char *label;
    DeviceConfigValueType valueType;
    const DeviceConfigEnumChoice *choices;
    uint8_t choiceCount;
    uint8_t endpoint;
    uint16_t clusterId;
    uint16_t attributeId;
    uint8_t zclDataType;
    bool manufacturerSpecific;
};

inline const char *deviceConfigValueTypeJsonId(DeviceConfigValueType valueType) {
    switch (valueType) {
        case DeviceConfigValueBool:
            return "bool";
        case DeviceConfigValueEnum:
            return "enum";
        case DeviceConfigValueUInt8:
            return "uint8";
        case DeviceConfigValueUInt16:
            return "uint16";
        case DeviceConfigValueInt8:
            return "int8";
        case DeviceConfigValueInt16:
            return "int16";
        default:
            return "uint8";
    }
}

constexpr uint8_t kZclDataTypeBool = 0x10;
constexpr uint8_t kZclDataTypeEnum8 = 0x30;
constexpr uint8_t kZclDataTypeU8 = 0x20;
constexpr uint8_t kZclDataTypeU16 = 0x21;
constexpr uint8_t kZclDataTypeInt8 = 0x28;
constexpr uint8_t kZclDataTypeBitmap8 = 0x18;

// Device-kind ids match ZigbeeDeviceType.h catalog (not only classic enum 0..3).
constexpr uint8_t kCfgTypeOnOff = ZigbeeDeviceTypeOnOff;
constexpr uint8_t kCfgTypeIasZone = ZigbeeDeviceTypeIasZone;
constexpr uint8_t kCfgTypeWindowCovering = ZigbeeDeviceTypeWindowCovering;
constexpr uint8_t kCfgTypeLevelControl = 11;
constexpr uint8_t kCfgTypeShadeConfiguration = 33;
constexpr uint8_t kCfgTypeDoorLock = 34;
constexpr uint8_t kCfgTypeThermostat = 36;
constexpr uint8_t kCfgTypeFanControl = 37;
constexpr uint8_t kCfgTypeThermostatUi = 39;
constexpr uint8_t kCfgTypeColorControl = 40;
constexpr uint8_t kCfgTypeBallast = 41;
constexpr uint8_t kCfgTypeOccupancy = 48;

constexpr uint16_t kZclClusterOnOff = 0x0006;
constexpr uint16_t kZclClusterLevel = 0x0008;
constexpr uint16_t kZclClusterShadeConfig = 0x0100;
constexpr uint16_t kZclClusterDoorLock = 0x0101;
constexpr uint16_t kZclClusterWindowCovering = 0x0102;
constexpr uint16_t kZclClusterThermostat = 0x0201;
constexpr uint16_t kZclClusterFanControl = 0x0202;
constexpr uint16_t kZclClusterThermostatUi = 0x0204;
constexpr uint16_t kZclClusterColorControl = 0x0300;
constexpr uint16_t kZclClusterBallast = 0x0301;
constexpr uint16_t kZclClusterOccupancy = 0x0406;
constexpr uint16_t kZclClusterIasZone = 0x0500;

constexpr uint16_t kZclAttrStartUpOnOff = 0x4003;
constexpr uint16_t kZclAttrStartUpCurrentLevel = 0x4000;
constexpr uint16_t kZclAttrStartUpColorTemp = 0x4010;
constexpr uint16_t kZclAttrWindowCoveringMode = 0x0017;
constexpr uint16_t kZclAttrLocalTempCalibration = 0x0010;
constexpr uint16_t kZclAttrFanModeSequence = 0x0001;
constexpr uint16_t kZclAttrIasZoneSensitivity = 0x0013;
constexpr uint16_t kZclAttrShadeMode = 0x0002;
constexpr uint16_t kZclAttrDoorLockOperatingMode = 0x0025;
constexpr uint16_t kZclAttrDoorLockSoundVolume = 0x0026;
constexpr uint16_t kZclAttrKeypadLockout = 0x0001;
constexpr uint16_t kZclAttrBallastMinLevel = 0x0010;
constexpr uint16_t kZclAttrBallastMaxLevel = 0x0011;
constexpr uint16_t kZclAttrOccupancySensitivity = 0x0030;
constexpr uint16_t kZclAttrTuyaPowerOn = 0x8002;
constexpr uint16_t kZclAttrTuyaIndicator = 0x8001;
constexpr uint16_t kZclAttrTuyaSwitchType = 0xD030;
constexpr uint16_t kZclAttrTuyaCoverMotorMode = 0x8000;
constexpr uint16_t kZclAttrTuyaCoverCalibration = 0xF001;
constexpr uint16_t kZclAttrTuyaCoverMotorReversal = 0xF002;
constexpr uint16_t kZclClusterAqaraOp = 0xFCC0;
constexpr uint16_t kZclAttrAqaraPowerOn = 0x0517;
constexpr uint16_t kZclAttrAqaraOperationMode = 0x0200;
constexpr uint16_t kZclAttrSonoffNetworkIndicator = 0x2001;

constexpr size_t kDeviceConfigMatchMax = 16;

inline bool deviceConfigModelMatches(const char *modelPrefix, const char *deviceModel) {
    if (modelPrefix == nullptr || modelPrefix[0] == '\0') {
        return true;
    }
    if (deviceModel == nullptr || deviceModel[0] == '\0') {
        return false;
    }
    const size_t prefixLen = strlen(modelPrefix);
    for (size_t index = 0; index < prefixLen; index++) {
        const char left = deviceModel[index];
        const char right = modelPrefix[index];
        if (left == '\0') {
            return false;
        }
        if (tolower((unsigned char)left) != tolower((unsigned char)right)) {
            return false;
        }
    }
    return true;
}

inline const DeviceConfigAttr *deviceConfigCatalog(size_t *count) {
    static const DeviceConfigEnumChoice kTuyaPowerOn[] = {
        {"Off", 0}, {"On", 1}, {"Memory", 2}
    };
    static const DeviceConfigEnumChoice kTuyaSwitchType[] = {
        {"Toggle", 0}, {"State", 1}, {"Momentary", 2}
    };
    static const DeviceConfigEnumChoice kTuyaIndicator[] = {
        {"Off", 0}, {"Status", 1}, {"Position", 2}
    };
    static const DeviceConfigEnumChoice kTuyaOnOff[] = {
        {"Off", 0}, {"On", 1}
    };
    static const DeviceConfigEnumChoice kAqaraPowerOn[] = {
        {"On", 0}, {"Previous", 1}, {"Off", 2}, {"Inverted", 3}
    };
    static const DeviceConfigEnumChoice kAqaraMode[] = {
        {"Decoupled", 0}, {"Control relay", 1}
    };
    static const DeviceConfigEnumChoice kStdPowerOn[] = {
        {"Off", 0}, {"On", 1}, {"Toggle", 2}, {"Previous", 0xFF}
    };
    static const DeviceConfigEnumChoice kIasSensitivity[] = {
        {"Low", 0}, {"Medium", 1}, {"High", 2}
    };
    static const DeviceConfigEnumChoice kFanModeSequence[] = {
        {"Low/Med/High", 0},
        {"Low/High", 1},
        {"Low/Med/High/Auto", 2},
        {"Low/High/Auto", 3},
        {"On/Auto", 4}
    };
    static const DeviceConfigEnumChoice kCoverModeSimple[] = {
        {"Normal", 0}, {"Motor reversed", 1}
    };
    static const DeviceConfigEnumChoice kTuyaMotorMode[] = {
        {"Strong", 0}, {"Weak", 1}
    };
    static const DeviceConfigEnumChoice kShadeMode[] = {
        {"Normal", 0}, {"Configure", 1}
    };
    static const DeviceConfigEnumChoice kDoorLockMode[] = {
        {"Normal", 0}, {"Vacation", 1}, {"Privacy", 2}, {"No RF lock/unlock", 3},
        {"Passage", 4}
    };
    static const DeviceConfigEnumChoice kDoorLockVolume[] = {
        {"Silent", 0}, {"Low", 1}, {"High", 2}
    };
    static const DeviceConfigEnumChoice kKeypadLockout[] = {
        {"Unlocked", 0}, {"Locked", 1}
    };

    // One row helper: manufacturer, type, id, label, valueType, choices, choiceCount,
    // cluster, attr, dataType, manuSpecific.
#define R(m, t, id, lab, vt, ch, cn, cl, at, dt, ms) \
    { (m), (t), nullptr, (id), (lab), (vt), (ch), (cn), 1, (cl), (at), (dt), (ms) }

    // Standard ZCL bundle used across brands (ZCL + Z2M/ZHA common exposes).
#define STD_KINDS(m)                                                                                  \
    R(m, kCfgTypeOnOff, "power_on_behavior", "Power-on behavior", DeviceConfigValueEnum, kStdPowerOn, 4, \
      kZclClusterOnOff, kZclAttrStartUpOnOff, kZclDataTypeEnum8, false),                              \
    R(m, kCfgTypeLevelControl, "power_on_behavior", "Power-on behavior", DeviceConfigValueEnum,       \
      kStdPowerOn, 4, kZclClusterOnOff, kZclAttrStartUpOnOff, kZclDataTypeEnum8, false),              \
    R(m, kCfgTypeLevelControl, "startup_current_level", "Startup current level",                      \
      DeviceConfigValueUInt8, nullptr, 0, kZclClusterLevel, kZclAttrStartUpCurrentLevel,              \
      kZclDataTypeU8, false),                                                                         \
    R(m, kCfgTypeColorControl, "power_on_behavior", "Power-on behavior", DeviceConfigValueEnum,       \
      kStdPowerOn, 4, kZclClusterOnOff, kZclAttrStartUpOnOff, kZclDataTypeEnum8, false),              \
    R(m, kCfgTypeColorControl, "startup_current_level", "Startup current level",                      \
      DeviceConfigValueUInt8, nullptr, 0, kZclClusterLevel, kZclAttrStartUpCurrentLevel,              \
      kZclDataTypeU8, false),                                                                         \
    R(m, kCfgTypeColorControl, "startup_color_temp", "Startup color temperature",                     \
      DeviceConfigValueUInt16, nullptr, 0, kZclClusterColorControl, kZclAttrStartUpColorTemp,          \
      kZclDataTypeU16, false),                                                                        \
    R(m, kCfgTypeWindowCovering, "window_covering_mode", "Window covering mode",                      \
      DeviceConfigValueEnum, kCoverModeSimple, 2, kZclClusterWindowCovering,                          \
      kZclAttrWindowCoveringMode, kZclDataTypeBitmap8, false),                                        \
    R(m, kCfgTypeShadeConfiguration, "shade_mode", "Shade mode", DeviceConfigValueEnum, kShadeMode, 2, \
      kZclClusterShadeConfig, kZclAttrShadeMode, kZclDataTypeEnum8, false),                           \
    R(m, kCfgTypeDoorLock, "operating_mode", "Operating mode", DeviceConfigValueEnum, kDoorLockMode, 5, \
      kZclClusterDoorLock, kZclAttrDoorLockOperatingMode, kZclDataTypeEnum8, false),                  \
    R(m, kCfgTypeDoorLock, "sound_volume", "Sound volume", DeviceConfigValueEnum, kDoorLockVolume, 3,  \
      kZclClusterDoorLock, kZclAttrDoorLockSoundVolume, kZclDataTypeEnum8, false),                    \
    R(m, kCfgTypeThermostat, "local_temperature_calibration", "Local temperature calibration",        \
      DeviceConfigValueInt8, nullptr, 0, kZclClusterThermostat, kZclAttrLocalTempCalibration,         \
      kZclDataTypeInt8, false),                                                                       \
    R(m, kCfgTypeThermostatUi, "keypad_lockout", "Keypad lockout", DeviceConfigValueEnum,             \
      kKeypadLockout, 2, kZclClusterThermostatUi, kZclAttrKeypadLockout, kZclDataTypeEnum8, false),   \
    R(m, kCfgTypeIasZone, "zone_sensitivity", "Zone sensitivity", DeviceConfigValueEnum,              \
      kIasSensitivity, 3, kZclClusterIasZone, kZclAttrIasZoneSensitivity, kZclDataTypeEnum8, false),  \
    R(m, kCfgTypeFanControl, "fan_mode_sequence", "Fan mode sequence", DeviceConfigValueEnum,         \
      kFanModeSequence, 5, kZclClusterFanControl, kZclAttrFanModeSequence, kZclDataTypeEnum8, false), \
    R(m, kCfgTypeBallast, "min_level", "Minimum level", DeviceConfigValueUInt8, nullptr, 0,           \
      kZclClusterBallast, kZclAttrBallastMinLevel, kZclDataTypeU8, false),                            \
    R(m, kCfgTypeBallast, "max_level", "Maximum level", DeviceConfigValueUInt8, nullptr, 0,           \
      kZclClusterBallast, kZclAttrBallastMaxLevel, kZclDataTypeU8, false),                            \
    R(m, kCfgTypeOccupancy, "occupancy_sensitivity", "Occupancy sensitivity", DeviceConfigValueUInt8, \
      nullptr, 0, kZclClusterOccupancy, kZclAttrOccupancySensitivity, kZclDataTypeU8, false)

    static const DeviceConfigAttr kCatalog[] = {
        // --- Tuya (manu-specific On/Off + curtain attrs from Z2M/ZHA TS130F family) ---
        R(DeviceManufacturerTuya, kCfgTypeOnOff, "power_on_behavior", "Power-on behavior",
          DeviceConfigValueEnum, kTuyaPowerOn, 3, kZclClusterOnOff, kZclAttrTuyaPowerOn,
          kZclDataTypeEnum8, true),
        R(DeviceManufacturerTuya, kCfgTypeOnOff, "switch_type", "Switch type", DeviceConfigValueEnum,
          kTuyaSwitchType, 3, kZclClusterOnOff, kZclAttrTuyaSwitchType, kZclDataTypeEnum8, true),
        R(DeviceManufacturerTuya, kCfgTypeOnOff, "indicator_mode", "Indicator mode",
          DeviceConfigValueEnum, kTuyaIndicator, 3, kZclClusterOnOff, kZclAttrTuyaIndicator,
          kZclDataTypeEnum8, true),
        R(DeviceManufacturerTuya, kCfgTypeLevelControl, "power_on_behavior", "Power-on behavior",
          DeviceConfigValueEnum, kTuyaPowerOn, 3, kZclClusterOnOff, kZclAttrTuyaPowerOn,
          kZclDataTypeEnum8, true),
        R(DeviceManufacturerTuya, kCfgTypeLevelControl, "startup_current_level", "Startup current level",
          DeviceConfigValueUInt8, nullptr, 0, kZclClusterLevel, kZclAttrStartUpCurrentLevel,
          kZclDataTypeU8, false),
        R(DeviceManufacturerTuya, kCfgTypeColorControl, "power_on_behavior", "Power-on behavior",
          DeviceConfigValueEnum, kTuyaPowerOn, 3, kZclClusterOnOff, kZclAttrTuyaPowerOn,
          kZclDataTypeEnum8, true),
        R(DeviceManufacturerTuya, kCfgTypeColorControl, "startup_current_level", "Startup current level",
          DeviceConfigValueUInt8, nullptr, 0, kZclClusterLevel, kZclAttrStartUpCurrentLevel,
          kZclDataTypeU8, false),
        R(DeviceManufacturerTuya, kCfgTypeColorControl, "startup_color_temp", "Startup color temperature",
          DeviceConfigValueUInt16, nullptr, 0, kZclClusterColorControl, kZclAttrStartUpColorTemp,
          kZclDataTypeU16, false),
        R(DeviceManufacturerTuya, kCfgTypeWindowCovering, "motor_reversal", "Motor reversal",
          DeviceConfigValueEnum, kTuyaOnOff, 2, kZclClusterWindowCovering,
          kZclAttrTuyaCoverMotorReversal, kZclDataTypeEnum8, false),
        R(DeviceManufacturerTuya, kCfgTypeWindowCovering, "calibration", "Calibration",
          DeviceConfigValueEnum, kTuyaOnOff, 2, kZclClusterWindowCovering,
          kZclAttrTuyaCoverCalibration, kZclDataTypeEnum8, false),
        R(DeviceManufacturerTuya, kCfgTypeWindowCovering, "motor_mode", "Motor mode",
          DeviceConfigValueEnum, kTuyaMotorMode, 2, kZclClusterWindowCovering,
          kZclAttrTuyaCoverMotorMode, kZclDataTypeEnum8, false),
        R(DeviceManufacturerTuya, kCfgTypeWindowCovering, "indicator_mode", "Indicator mode",
          DeviceConfigValueEnum, kTuyaIndicator, 3, kZclClusterOnOff, kZclAttrTuyaIndicator,
          kZclDataTypeEnum8, true),
        R(DeviceManufacturerTuya, kCfgTypeWindowCovering, "switch_type", "Switch type",
          DeviceConfigValueEnum, kTuyaSwitchType, 3, kZclClusterOnOff, kZclAttrTuyaSwitchType,
          kZclDataTypeEnum8, true),
        R(DeviceManufacturerTuya, kCfgTypeShadeConfiguration, "shade_mode", "Shade mode",
          DeviceConfigValueEnum, kShadeMode, 2, kZclClusterShadeConfig, kZclAttrShadeMode,
          kZclDataTypeEnum8, false),
        R(DeviceManufacturerTuya, kCfgTypeDoorLock, "operating_mode", "Operating mode",
          DeviceConfigValueEnum, kDoorLockMode, 5, kZclClusterDoorLock, kZclAttrDoorLockOperatingMode,
          kZclDataTypeEnum8, false),
        R(DeviceManufacturerTuya, kCfgTypeDoorLock, "sound_volume", "Sound volume",
          DeviceConfigValueEnum, kDoorLockVolume, 3, kZclClusterDoorLock, kZclAttrDoorLockSoundVolume,
          kZclDataTypeEnum8, false),
        R(DeviceManufacturerTuya, kCfgTypeThermostat, "local_temperature_calibration",
          "Local temperature calibration", DeviceConfigValueInt8, nullptr, 0, kZclClusterThermostat,
          kZclAttrLocalTempCalibration, kZclDataTypeInt8, false),
        R(DeviceManufacturerTuya, kCfgTypeThermostatUi, "keypad_lockout", "Keypad lockout",
          DeviceConfigValueEnum, kKeypadLockout, 2, kZclClusterThermostatUi, kZclAttrKeypadLockout,
          kZclDataTypeEnum8, false),
        R(DeviceManufacturerTuya, kCfgTypeIasZone, "zone_sensitivity", "Zone sensitivity",
          DeviceConfigValueEnum, kIasSensitivity, 3, kZclClusterIasZone, kZclAttrIasZoneSensitivity,
          kZclDataTypeEnum8, false),
        R(DeviceManufacturerTuya, kCfgTypeFanControl, "fan_mode_sequence", "Fan mode sequence",
          DeviceConfigValueEnum, kFanModeSequence, 5, kZclClusterFanControl, kZclAttrFanModeSequence,
          kZclDataTypeEnum8, false),
        R(DeviceManufacturerTuya, kCfgTypeBallast, "min_level", "Minimum level", DeviceConfigValueUInt8,
          nullptr, 0, kZclClusterBallast, kZclAttrBallastMinLevel, kZclDataTypeU8, false),
        R(DeviceManufacturerTuya, kCfgTypeBallast, "max_level", "Maximum level", DeviceConfigValueUInt8,
          nullptr, 0, kZclClusterBallast, kZclAttrBallastMaxLevel, kZclDataTypeU8, false),
        R(DeviceManufacturerTuya, kCfgTypeOccupancy, "occupancy_sensitivity", "Occupancy sensitivity",
          DeviceConfigValueUInt8, nullptr, 0, kZclClusterOccupancy, kZclAttrOccupancySensitivity,
          kZclDataTypeU8, false),

        // --- Aqara (manu-specific FCC0 + standard kinds) ---
        R(DeviceManufacturerAqara, kCfgTypeOnOff, "power_on_behavior", "Power-on behavior",
          DeviceConfigValueEnum, kAqaraPowerOn, 4, kZclClusterAqaraOp, kZclAttrAqaraPowerOn,
          kZclDataTypeU8, true),
        R(DeviceManufacturerAqara, kCfgTypeOnOff, "operation_mode", "Operation mode",
          DeviceConfigValueEnum, kAqaraMode, 2, kZclClusterAqaraOp, kZclAttrAqaraOperationMode,
          kZclDataTypeU8, true),
        R(DeviceManufacturerAqara, kCfgTypeLevelControl, "power_on_behavior", "Power-on behavior",
          DeviceConfigValueEnum, kAqaraPowerOn, 4, kZclClusterAqaraOp, kZclAttrAqaraPowerOn,
          kZclDataTypeU8, true),
        R(DeviceManufacturerAqara, kCfgTypeLevelControl, "startup_current_level", "Startup current level",
          DeviceConfigValueUInt8, nullptr, 0, kZclClusterLevel, kZclAttrStartUpCurrentLevel,
          kZclDataTypeU8, false),
        R(DeviceManufacturerAqara, kCfgTypeColorControl, "power_on_behavior", "Power-on behavior",
          DeviceConfigValueEnum, kAqaraPowerOn, 4, kZclClusterAqaraOp, kZclAttrAqaraPowerOn,
          kZclDataTypeU8, true),
        R(DeviceManufacturerAqara, kCfgTypeColorControl, "startup_color_temp", "Startup color temperature",
          DeviceConfigValueUInt16, nullptr, 0, kZclClusterColorControl, kZclAttrStartUpColorTemp,
          kZclDataTypeU16, false),
        R(DeviceManufacturerAqara, kCfgTypeWindowCovering, "window_covering_mode", "Window covering mode",
          DeviceConfigValueEnum, kCoverModeSimple, 2, kZclClusterWindowCovering,
          kZclAttrWindowCoveringMode, kZclDataTypeBitmap8, false),
        R(DeviceManufacturerAqara, kCfgTypeShadeConfiguration, "shade_mode", "Shade mode",
          DeviceConfigValueEnum, kShadeMode, 2, kZclClusterShadeConfig, kZclAttrShadeMode,
          kZclDataTypeEnum8, false),
        R(DeviceManufacturerAqara, kCfgTypeDoorLock, "operating_mode", "Operating mode",
          DeviceConfigValueEnum, kDoorLockMode, 5, kZclClusterDoorLock, kZclAttrDoorLockOperatingMode,
          kZclDataTypeEnum8, false),
        R(DeviceManufacturerAqara, kCfgTypeDoorLock, "sound_volume", "Sound volume",
          DeviceConfigValueEnum, kDoorLockVolume, 3, kZclClusterDoorLock, kZclAttrDoorLockSoundVolume,
          kZclDataTypeEnum8, false),
        R(DeviceManufacturerAqara, kCfgTypeThermostat, "local_temperature_calibration",
          "Local temperature calibration", DeviceConfigValueInt8, nullptr, 0, kZclClusterThermostat,
          kZclAttrLocalTempCalibration, kZclDataTypeInt8, false),
        R(DeviceManufacturerAqara, kCfgTypeThermostatUi, "keypad_lockout", "Keypad lockout",
          DeviceConfigValueEnum, kKeypadLockout, 2, kZclClusterThermostatUi, kZclAttrKeypadLockout,
          kZclDataTypeEnum8, false),
        R(DeviceManufacturerAqara, kCfgTypeIasZone, "zone_sensitivity", "Zone sensitivity",
          DeviceConfigValueEnum, kIasSensitivity, 3, kZclClusterIasZone, kZclAttrIasZoneSensitivity,
          kZclDataTypeEnum8, false),
        R(DeviceManufacturerAqara, kCfgTypeFanControl, "fan_mode_sequence", "Fan mode sequence",
          DeviceConfigValueEnum, kFanModeSequence, 5, kZclClusterFanControl, kZclAttrFanModeSequence,
          kZclDataTypeEnum8, false),
        R(DeviceManufacturerAqara, kCfgTypeBallast, "min_level", "Minimum level", DeviceConfigValueUInt8,
          nullptr, 0, kZclClusterBallast, kZclAttrBallastMinLevel, kZclDataTypeU8, false),
        R(DeviceManufacturerAqara, kCfgTypeBallast, "max_level", "Maximum level", DeviceConfigValueUInt8,
          nullptr, 0, kZclClusterBallast, kZclAttrBallastMaxLevel, kZclDataTypeU8, false),
        R(DeviceManufacturerAqara, kCfgTypeOccupancy, "occupancy_sensitivity", "Occupancy sensitivity",
          DeviceConfigValueUInt8, nullptr, 0, kZclClusterOccupancy, kZclAttrOccupancySensitivity,
          kZclDataTypeU8, false),

        // --- Sonoff: network_indicator before STD so On/Off rows stay contiguous ---
        R(DeviceManufacturerSonoff, kCfgTypeOnOff, "network_indicator", "Network indicator",
          DeviceConfigValueBool, nullptr, 0, kZclClusterOnOff, kZclAttrSonoffNetworkIndicator,
          kZclDataTypeBool, true),
        STD_KINDS(DeviceManufacturerSonoff),

        // --- Remaining project manufacturers: full standard kind matrix ---
        STD_KINDS(DeviceManufacturerPhilipsHue),
        STD_KINDS(DeviceManufacturerIkea),
        STD_KINDS(DeviceManufacturerLedvance),
        STD_KINDS(DeviceManufacturerInnr),
        STD_KINDS(DeviceManufacturerGledopto),
        STD_KINDS(DeviceManufacturerSengled),
        STD_KINDS(DeviceManufacturerThirdReality),
        STD_KINDS(DeviceManufacturerHeiman),
        STD_KINDS(DeviceManufacturerLegrand),
        STD_KINDS(DeviceManufacturerSchneider),
        STD_KINDS(DeviceManufacturerNamron),
        STD_KINDS(DeviceManufacturerDevelco),
    };
#undef STD_KINDS
#undef R

    if (count != nullptr) {
        *count = sizeof(kCatalog) / sizeof(kCatalog[0]);
    }
    return kCatalog;
}

inline size_t deviceConfigCollectFor(
    uint8_t manufacturer,
    uint8_t zigbeeType,
    const char *model,
    const DeviceConfigAttr **outAttrs,
    size_t outMax
) {
    if (outAttrs == nullptr || outMax == 0) {
        return 0;
    }
    size_t total = 0;
    const DeviceConfigAttr *catalog = deviceConfigCatalog(&total);
    size_t matched = 0;
    for (size_t index = 0; index < total && matched < outMax; index++) {
        if (catalog[index].manufacturer != manufacturer || catalog[index].zigbeeType != zigbeeType) {
            continue;
        }
        if (!deviceConfigModelMatches(catalog[index].modelPrefix, model)) {
            continue;
        }
        outAttrs[matched++] = &catalog[index];
    }
    return matched;
}

inline size_t deviceConfigListFor(
    uint8_t manufacturer,
    uint8_t zigbeeType,
    const char *model,
    const DeviceConfigAttr **outFirst
) {
    size_t total = 0;
    const DeviceConfigAttr *catalog = deviceConfigCatalog(&total);
    const DeviceConfigAttr *first = nullptr;
    size_t contiguous = 0;
    for (size_t index = 0; index < total; index++) {
        if (catalog[index].manufacturer != manufacturer
            || catalog[index].zigbeeType != zigbeeType
            || !deviceConfigModelMatches(catalog[index].modelPrefix, model)) {
            if (first != nullptr) {
                break;
            }
            continue;
        }
        if (first == nullptr) {
            first = &catalog[index];
        }
        contiguous++;
    }
    if (outFirst != nullptr) {
        *outFirst = first;
    }
    return contiguous;
}

inline size_t deviceConfigListFor(
    uint8_t manufacturer,
    uint8_t zigbeeType,
    const DeviceConfigAttr **outFirst
) {
    return deviceConfigListFor(manufacturer, zigbeeType, nullptr, outFirst);
}

inline const DeviceConfigAttr *deviceConfigFindAttr(
    uint8_t manufacturer,
    uint8_t zigbeeType,
    const char *model,
    const char *configId
) {
    if (configId == nullptr) {
        return nullptr;
    }
    const DeviceConfigAttr *matched[kDeviceConfigMatchMax];
    const size_t count = deviceConfigCollectFor(
        manufacturer,
        zigbeeType,
        model,
        matched,
        kDeviceConfigMatchMax
    );
    for (size_t index = 0; index < count; index++) {
        if (strcmp(matched[index]->id, configId) == 0) {
            return matched[index];
        }
    }
    return nullptr;
}

inline bool deviceHasConfigurableAttrs(
    uint8_t transport,
    uint8_t manufacturer,
    uint8_t zigbeeType,
    const char *model = nullptr
) {
    if (clampDeviceTransport(transport) == DeviceTransportMqtt) {
        return false;
    }
    const DeviceConfigAttr *matched[1];
    return deviceConfigCollectFor(manufacturer, zigbeeType, model, matched, 1) > 0;
}

inline bool deviceConfigCatalogHasForbiddenRuntimeIds() {
    size_t total = 0;
    const DeviceConfigAttr *catalog = deviceConfigCatalog(&total);
    for (size_t index = 0; index < total; index++) {
        const char *configId = catalog[index].id;
        if (configId == nullptr) {
            continue;
        }
        if (strcmp(configId, "status") == 0
            || strcmp(configId, "position") == 0
            || strcmp(configId, "battery") == 0
            || strcmp(configId, "rssi") == 0
            || strcmp(configId, "on_off") == 0
            || strcmp(configId, "onOff") == 0) {
            return true;
        }
    }
    return false;
}
