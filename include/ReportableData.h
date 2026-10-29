#pragma once

#include "DeviceManufacturer.h"
#include "JsonField.h"
#include "UserDataPayload.h"
#include "ZigbeeCluster.h"
#include "ZigbeeDeviceType.h"

#include <Arduino.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

constexpr uint8_t kReportableDataMaxCount = 16;
constexpr size_t kReportableValueTextMaxChars = 31;
constexpr size_t kReportableValueTextMaxLen = kReportableValueTextMaxChars + 1;

enum ReportableDataType : uint8_t {
    ReportableDataTypeInt = 0,
    ReportableDataTypeFloat = 1,
    ReportableDataTypeBool = 2,
    ReportableDataTypeString = 3
};

struct __attribute__((packed)) ReportableDataDef {
    uint16_t cluster;
    uint16_t attribute;
    uint8_t type;
};

inline ReportableDataType reportableDataTypeFromText(const char *text) {
    if (text == nullptr) {
        return ReportableDataTypeString;
    }
    if (strcasecmp(text, "int") == 0) {
        return ReportableDataTypeInt;
    }
    if (strcasecmp(text, "float") == 0) {
        return ReportableDataTypeFloat;
    }
    if (strcasecmp(text, "bool") == 0) {
        return ReportableDataTypeBool;
    }
    return ReportableDataTypeString;
}

inline const char *reportableDataTypeJsonId(ReportableDataType type) {
    switch (type) {
        case ReportableDataTypeInt:
            return "int";
        case ReportableDataTypeFloat:
            return "float";
        case ReportableDataTypeBool:
            return "bool";
        default:
            return "string";
    }
}

inline void reportableDataClear(ReportableDataDef **rows, uint8_t *rowCount) {
    if (rows != nullptr) {
        free(*rows);
        *rows = nullptr;
    }
    if (rowCount != nullptr) {
        *rowCount = 0;
    }
}

inline bool reportableDataCopy(
    ReportableDataDef **destinationRows,
    uint8_t *destinationCount,
    const ReportableDataDef *sourceRows,
    uint8_t sourceCount
) {
    if (destinationRows == nullptr || destinationCount == nullptr) {
        return false;
    }
    reportableDataClear(destinationRows, destinationCount);
    if (sourceCount == 0 || sourceRows == nullptr) {
        return true;
    }
    if (sourceCount > kReportableDataMaxCount) {
        sourceCount = kReportableDataMaxCount;
    }
    ReportableDataDef *copy =
        (ReportableDataDef *)malloc((size_t)sourceCount * sizeof(ReportableDataDef));
    if (copy == nullptr) {
        return false;
    }
    memcpy(copy, sourceRows, (size_t)sourceCount * sizeof(ReportableDataDef));
    *destinationRows = copy;
    *destinationCount = sourceCount;
    return true;
}

inline int reportableDataFindIndex(
    const ReportableDataDef *rows,
    uint8_t rowCount,
    uint16_t cluster,
    uint16_t attribute
) {
    if (rows == nullptr) {
        return -1;
    }
    // Exact (cluster, attribute) first.
    for (uint8_t index = 0; index < rowCount; index++) {
        if (rows[index].cluster == cluster && rows[index].attribute == attribute) {
            return (int)index;
        }
    }
    // attr 0x0000 (or unspecified) means every attribute of that cluster has the same sense.
    for (uint8_t index = 0; index < rowCount; index++) {
        if (rows[index].cluster == cluster && rows[index].attribute == 0x0000) {
            return (int)index;
        }
    }
    return -1;
}

inline bool reportableDataAppendUnique(
    ReportableDataDef *rows,
    uint8_t *rowCount,
    uint16_t cluster,
    uint16_t attribute,
    ReportableDataType type
) {
    if (rows == nullptr || rowCount == nullptr) {
        return false;
    }
    if (reportableDataFindIndex(rows, *rowCount, cluster, attribute) >= 0) {
        return true;
    }
    if (*rowCount >= kReportableDataMaxCount) {
        return false;
    }
    rows[*rowCount].cluster = cluster;
    rows[*rowCount].attribute = attribute;
    rows[*rowCount].type = (uint8_t)type;
    (*rowCount)++;
    return true;
}

inline void reportableDataSeedManufacturer(uint8_t manufacturer, ReportableDataDef *rows, uint8_t *rowCount) {
    if (rows == nullptr || rowCount == nullptr) {
        return;
    }
    if (manufacturer == DeviceManufacturerZbeacon) {
        reportableDataAppendUnique(
            rows,
            rowCount,
            kZigbeeClusterTemperature,
            0x0000,
            ReportableDataTypeFloat
        );
        reportableDataAppendUnique(
            rows,
            rowCount,
            kZigbeeClusterHumidity,
            0x0000,
            ReportableDataTypeFloat
        );
        reportableDataAppendUnique(
            rows,
            rowCount,
            kZigbeeClusterPowerConfig,
            kZigbeeAttrBatteryPercentageRemaining,
            ReportableDataTypeInt
        );
    }
}

inline void reportableDataSeedDiscovered(uint8_t zigbeeType, ReportableDataDef *rows, uint8_t *rowCount) {
    if (rows == nullptr || rowCount == nullptr) {
        return;
    }
    const uint16_t typeCluster = zigbeeDeviceTypeInfo(zigbeeType)->clusterId;
    if (typeCluster == kZigbeeClusterTemperature) {
        reportableDataAppendUnique(
            rows,
            rowCount,
            kZigbeeClusterTemperature,
            0x0000,
            ReportableDataTypeFloat
        );
    } else if (typeCluster == kZigbeeClusterHumidity) {
        reportableDataAppendUnique(
            rows,
            rowCount,
            kZigbeeClusterHumidity,
            0x0000,
            ReportableDataTypeFloat
        );
    } else if (typeCluster == kZigbeeClusterIlluminance || typeCluster == 0x0401) {
        reportableDataAppendUnique(
            rows,
            rowCount,
            typeCluster,
            0x0000,
            ReportableDataTypeFloat
        );
    } else if (typeCluster == kZigbeeClusterPressure) {
        reportableDataAppendUnique(
            rows,
            rowCount,
            kZigbeeClusterPressure,
            0x0000,
            ReportableDataTypeFloat
        );
    }
    if (zigbeeDeviceTypeIsMeasurement(zigbeeType)
        || typeCluster == kZigbeeClusterOccupancy
        || typeCluster == kZigbeeClusterIasZone) {
        reportableDataAppendUnique(
            rows,
            rowCount,
            kZigbeeClusterPowerConfig,
            kZigbeeAttrBatteryPercentageRemaining,
            ReportableDataTypeInt
        );
    }
}

inline bool reportableDataAssignSeeded(
    ReportableDataDef **rows,
    uint8_t *rowCount,
    uint8_t manufacturer,
    uint8_t zigbeeType
) {
    if (rows == nullptr || rowCount == nullptr) {
        return false;
    }
    ReportableDataDef scratch[kReportableDataMaxCount];
    uint8_t scratchCount = 0;
    reportableDataSeedManufacturer(manufacturer, scratch, &scratchCount);
    reportableDataSeedDiscovered(zigbeeType, scratch, &scratchCount);
    return reportableDataCopy(rows, rowCount, scratch, scratchCount);
}

inline bool reportableDataConvertZclValue(
    uint16_t cluster,
    uint16_t attribute,
    uint32_t rawValue,
    ReportableDataType type,
    char *out,
    size_t outSize
) {
    if (out == nullptr || outSize == 0) {
        return false;
    }
    out[0] = '\0';
    if (cluster == kZigbeeClusterTemperature && attribute == 0x0000) {
        const uint16_t word = (uint16_t)(rawValue & 0xFFFF);
        if (word == 0x8000) {
            return false;
        }
        const float celsius = ((int16_t)word) / 100.0f;
        if (type == ReportableDataTypeInt) {
            snprintf(out, outSize, "%d", (int)(celsius + (celsius >= 0 ? 0.5f : -0.5f)));
        } else if (type == ReportableDataTypeBool) {
            snprintf(out, outSize, "%s", celsius != 0.0f ? "true" : "false");
        } else if (type == ReportableDataTypeString) {
            snprintf(out, outSize, "%.2f", (double)celsius);
        } else {
            snprintf(out, outSize, "%.2f", (double)celsius);
        }
        return true;
    }
    if (cluster == kZigbeeClusterHumidity && attribute == 0x0000) {
        const uint16_t word = (uint16_t)(rawValue & 0xFFFF);
        if (word == 0xFFFF) {
            return false;
        }
        const float percent = word / 100.0f;
        if (type == ReportableDataTypeInt) {
            snprintf(out, outSize, "%u", (unsigned)(percent + 0.5f));
        } else if (type == ReportableDataTypeBool) {
            snprintf(out, outSize, "%s", percent != 0.0f ? "true" : "false");
        } else if (type == ReportableDataTypeString) {
            snprintf(out, outSize, "%.2f", (double)percent);
        } else {
            snprintf(out, outSize, "%.2f", (double)percent);
        }
        return true;
    }
    if (cluster == kZigbeeClusterPowerConfig && attribute == kZigbeeAttrBatteryPercentageRemaining) {
        if (!zigbeeBatteryPercentageRemainingValid(rawValue)) {
            return false;
        }
        const unsigned percent = zigbeeBatteryPercentageFromRemaining(rawValue);
        if (type == ReportableDataTypeFloat) {
            snprintf(out, outSize, "%u.0", percent);
        } else if (type == ReportableDataTypeBool) {
            snprintf(out, outSize, "%s", percent != 0 ? "true" : "false");
        } else if (type == ReportableDataTypeString) {
            snprintf(out, outSize, "%u", percent);
        } else {
            snprintf(out, outSize, "%u", percent);
        }
        return true;
    }
    if (cluster == kZigbeeClusterPressure && attribute == 0x0000) {
        const uint16_t word = (uint16_t)(rawValue & 0xFFFF);
        if (word == 0x8000) {
            return false;
        }
        const float hPa = ((int16_t)word) / 10.0f;
        if (type == ReportableDataTypeInt) {
            snprintf(out, outSize, "%d", (int)(hPa + (hPa >= 0 ? 0.5f : -0.5f)));
        } else if (type == ReportableDataTypeBool) {
            snprintf(out, outSize, "%s", hPa != 0.0f ? "true" : "false");
        } else {
            snprintf(out, outSize, "%.1f", (double)hPa);
        }
        return true;
    }
    // Unknown pair: coerce raw with no scale.
    if (type == ReportableDataTypeBool) {
        snprintf(out, outSize, "%s", rawValue != 0 ? "true" : "false");
        return true;
    }
    if (type == ReportableDataTypeFloat) {
        snprintf(out, outSize, "%lu.0", (unsigned long)rawValue);
        return true;
    }
    if (type == ReportableDataTypeString) {
        snprintf(out, outSize, "0x%lX", (unsigned long)rawValue);
        return true;
    }
    snprintf(out, outSize, "%lu", (unsigned long)rawValue);
    return true;
}

inline String reportableDataToJson(const ReportableDataDef *rows, uint8_t rowCount) {
    String json = "[";
    if (rows == nullptr) {
        json += "]";
        return json;
    }
    for (uint8_t index = 0; index < rowCount; index++) {
        if (index > 0) {
            json += ",";
        }
        char clusterText[12];
        char attributeText[12];
        snprintf(clusterText, sizeof(clusterText), "0x%04X", (unsigned)rows[index].cluster);
        snprintf(attributeText, sizeof(attributeText), "0x%04X", (unsigned)rows[index].attribute);
        json += "{\"cluster\":\"";
        json += clusterText;
        json += "\",\"attribute\":\"";
        json += attributeText;
        json += "\",\"type\":\"";
        json += reportableDataTypeJsonId((ReportableDataType)rows[index].type);
        json += "\"}";
    }
    json += "]";
    return json;
}

inline uint16_t reportableDataParseIdToken(const char *text) {
    if (text == nullptr || text[0] == '\0') {
        return 0;
    }
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    return (uint16_t)strtoul(text, nullptr, 0);
}

inline const char *reportableDataFindJsonObjectEnd(const char *objectStart) {
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

inline bool reportableDataAssignFromJson(
    ReportableDataDef **rows,
    uint8_t *rowCount,
    const char *jsonArray
) {
    if (rows == nullptr || rowCount == nullptr) {
        return false;
    }
    reportableDataClear(rows, rowCount);
    if (jsonArray == nullptr || jsonArray[0] == '\0') {
        return true;
    }
    ReportableDataDef parsed[kReportableDataMaxCount];
    uint8_t count = 0;
    const char *cursor = strchr(jsonArray, '[');
    if (cursor == nullptr) {
        return true;
    }
    cursor++;
    while (*cursor != '\0' && *cursor != ']' && count < kReportableDataMaxCount) {
        while (*cursor == ' ' || *cursor == '\t' || *cursor == ',' || *cursor == '\n' || *cursor == '\r') {
            cursor++;
        }
        if (*cursor == ']' || *cursor == '\0') {
            break;
        }
        if (*cursor != '{') {
            break;
        }
        const char *objectEnd = reportableDataFindJsonObjectEnd(cursor);
        if (objectEnd == nullptr) {
            break;
        }
        String object = String(cursor).substring(0, (unsigned int)(objectEnd - cursor + 1));
        String clusterText;
        String attributeText;
        String typeText;
        extractJsonString(object.c_str(), "cluster", clusterText);
        extractJsonString(object.c_str(), "attribute", attributeText);
        if (attributeText.length() == 0) {
            extractJsonString(object.c_str(), "attr", attributeText);
        }
        extractJsonString(object.c_str(), "type", typeText);
        int clusterInt = 0;
        int attributeInt = 0;
        if (clusterText.length() == 0) {
            extractJsonInt(object.c_str(), "cluster", clusterInt);
        } else {
            clusterInt = (int)reportableDataParseIdToken(clusterText.c_str());
        }
        if (attributeText.length() == 0) {
            if (!extractJsonInt(object.c_str(), "attribute", attributeInt)) {
                extractJsonInt(object.c_str(), "attr", attributeInt);
            }
        } else {
            attributeInt = (int)reportableDataParseIdToken(attributeText.c_str());
        }
        parsed[count].cluster = (uint16_t)clusterInt;
        parsed[count].attribute = (uint16_t)attributeInt;
        parsed[count].type = (uint8_t)reportableDataTypeFromText(typeText.c_str());
        count++;
        cursor = objectEnd + 1;
    }
    return reportableDataCopy(rows, rowCount, parsed, count);
}
