#pragma once

#include "JsonField.h"

#include <Arduino.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

constexpr uint8_t kUserDataFieldMaxCount = 24;
// Field name: 20 chars + NUL.
constexpr size_t kUserDataFieldNameMaxChars = 20;
constexpr size_t kUserDataFieldNameMaxLen = kUserDataFieldNameMaxChars + 1;
// String attribute value: 15 chars + NUL.
constexpr size_t kUserDataStringValueMaxChars = 15;
constexpr size_t kUserDataStringValueMaxLen = kUserDataStringValueMaxChars + 1;
// Packed schema row: name[20]+NUL + nullable + type.
constexpr size_t kUserDataFieldPackedBytes =
    kUserDataFieldNameMaxChars + 1 + 1 + 1;
constexpr size_t kUserDataFieldsMaxBytes =
    kUserDataFieldPackedBytes * kUserDataFieldMaxCount;
constexpr size_t kUserDataValuesMaxBytes =
    kUserDataFieldMaxCount * kUserDataStringValueMaxLen;

constexpr uint16_t kUserDataClusterId = 0xFC00;
constexpr uint16_t kUserDataAttributeId = 0x0001;

enum UserDataFieldType : uint8_t {
    UserDataFieldTypeInt = 0,
    UserDataFieldTypeFloat = 1,
    UserDataFieldTypeBool = 2,
    UserDataFieldTypeString = 3
};

struct __attribute__((packed)) UserDataFieldDef {
    uint8_t nullable;
    uint8_t type;
    char name[kUserDataFieldNameMaxLen];
};

static_assert(sizeof(UserDataFieldDef) == kUserDataFieldPackedBytes, "userData field pack size");
static_assert(kUserDataFieldsMaxBytes == 552, "userData schema max bytes");
static_assert(kUserDataValuesMaxBytes == 384, "userData values max bytes");

inline UserDataFieldType userDataFieldTypeFromText(const char *text) {
    if (text == nullptr) {
        return UserDataFieldTypeString;
    }
    if (strcasecmp(text, "int") == 0) {
        return UserDataFieldTypeInt;
    }
    if (strcasecmp(text, "float") == 0) {
        return UserDataFieldTypeFloat;
    }
    if (strcasecmp(text, "bool") == 0) {
        return UserDataFieldTypeBool;
    }
    return UserDataFieldTypeString;
}

inline const char *userDataFieldTypeJsonId(UserDataFieldType type) {
    switch (type) {
        case UserDataFieldTypeInt:
            return "int";
        case UserDataFieldTypeFloat:
            return "float";
        case UserDataFieldTypeBool:
            return "bool";
        default:
            return "string";
    }
}

inline void userDataCopyFieldName(char *destination, const char *source) {
    if (destination == nullptr) {
        return;
    }
    destination[0] = '\0';
    if (source == nullptr) {
        return;
    }
    size_t copyLen = 0;
    while (source[copyLen] != '\0' && copyLen < kUserDataFieldNameMaxChars) {
        destination[copyLen] = source[copyLen];
        copyLen++;
    }
    destination[copyLen] = '\0';
}

inline void userDataTruncateStringValue(String *value) {
    if (value == nullptr) {
        return;
    }
    if (value->length() > kUserDataStringValueMaxChars) {
        *value = value->substring(0, (unsigned int)kUserDataStringValueMaxChars);
    }
}

inline void userDataClearFieldDefs(UserDataFieldDef **fields, uint8_t *fieldCount) {
    if (fields != nullptr) {
        free(*fields);
        *fields = nullptr;
    }
    if (fieldCount != nullptr) {
        *fieldCount = 0;
    }
}

inline bool userDataCopyFieldDefs(
    UserDataFieldDef **destinationFields,
    uint8_t *destinationCount,
    const UserDataFieldDef *sourceFields,
    uint8_t sourceCount
) {
    if (destinationFields == nullptr || destinationCount == nullptr) {
        return false;
    }
    userDataClearFieldDefs(destinationFields, destinationCount);
    if (sourceCount == 0 || sourceFields == nullptr) {
        return true;
    }
    if (sourceCount > kUserDataFieldMaxCount) {
        sourceCount = kUserDataFieldMaxCount;
    }
    UserDataFieldDef *copy =
        (UserDataFieldDef *)malloc((size_t)sourceCount * sizeof(UserDataFieldDef));
    if (copy == nullptr) {
        return false;
    }
    memcpy(copy, sourceFields, (size_t)sourceCount * sizeof(UserDataFieldDef));
    *destinationFields = copy;
    *destinationCount = sourceCount;
    return true;
}

inline const char *userDataFindJsonObjectEnd(const char *objectStart) {
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

inline uint8_t userDataParseFieldsJson(
    const char *json,
    UserDataFieldDef *outFields,
    uint8_t maxCount
) {
    if (json == nullptr || outFields == nullptr || maxCount == 0) {
        return 0;
    }
    uint8_t count = 0;
    const char *cursor = json;
    while (cursor != nullptr && *cursor != '\0' && count < maxCount) {
        const char *objectStart = strchr(cursor, '{');
        if (objectStart == nullptr) {
            break;
        }
        const char *objectEnd = userDataFindJsonObjectEnd(objectStart);
        if (objectEnd == nullptr) {
            break;
        }
        String object = String(objectStart).substring(0, (unsigned int)(objectEnd - objectStart + 1));
        String nameText;
        String typeText;
        bool nullable = false;
        extractJsonString(object.c_str(), "name", nameText);
        extractJsonString(object.c_str(), "type", typeText);
        extractJsonBool(object.c_str(), "nullable", nullable);
        nameText.trim();
        UserDataFieldDef &field = outFields[count];
        memset(&field, 0, sizeof(field));
        field.nullable = nullable ? 1 : 0;
        field.type = (uint8_t)userDataFieldTypeFromText(typeText.c_str());
        userDataCopyFieldName(field.name, nameText.c_str());
        count++;
        cursor = objectEnd + 1;
    }
    return count;
}

inline bool userDataAssignFieldsFromJson(
    UserDataFieldDef **fields,
    uint8_t *fieldCount,
    const char *json
) {
    if (fields == nullptr || fieldCount == nullptr) {
        return false;
    }
    UserDataFieldDef parsed[kUserDataFieldMaxCount];
    const uint8_t count = userDataParseFieldsJson(json, parsed, kUserDataFieldMaxCount);
    return userDataCopyFieldDefs(fields, fieldCount, parsed, count);
}

inline String userDataFieldsToJson(const UserDataFieldDef *fields, uint8_t fieldCount) {
    String json = "[";
    if (fields == nullptr) {
        fieldCount = 0;
    }
    for (uint8_t index = 0; index < fieldCount; index++) {
        if (index > 0) {
            json += ",";
        }
        json += "{\"nullable\":";
        json += fields[index].nullable ? "true" : "false";
        json += ",\"name\":\"";
        appendJsonEscaped(json, fields[index].name, kUserDataFieldNameMaxChars);
        json += "\",\"type\":\"";
        json += userDataFieldTypeJsonId((UserDataFieldType)fields[index].type);
        json += "\"}";
    }
    json += "]";
    return json;
}

inline bool userDataSlotIsNullToken(const String &token) {
    String trimmed = token;
    trimmed.trim();
    if (trimmed.length() == 0) {
        return true;
    }
    return trimmed.equalsIgnoreCase("NULL");
}

inline bool userDataParseBoolToken(const String &token, bool *outValue) {
    if (outValue == nullptr) {
        return false;
    }
    String trimmed = token;
    trimmed.trim();
    if (trimmed.equalsIgnoreCase("true") || trimmed.equalsIgnoreCase("on")) {
        *outValue = true;
        return true;
    }
    if (trimmed.equalsIgnoreCase("false") || trimmed.equalsIgnoreCase("off")) {
        *outValue = false;
        return true;
    }
    return false;
}

inline bool userDataExtractBraceList(const char *message, String *outBraceList) {
    if (message == nullptr || outBraceList == nullptr) {
        return false;
    }
    const char *cursor = message;
    while (*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }
    if (*cursor == '{') {
        int depth = 0;
        bool inString = false;
        bool escape = false;
        for (const char *scan = cursor; *scan != '\0'; scan++) {
            const char character = *scan;
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
            if (character == '{') {
                depth++;
                continue;
            }
            if (character == '}') {
                depth--;
                if (depth == 0) {
                    *outBraceList = String(cursor).substring(0, (unsigned int)(scan - cursor + 1));
                    return true;
                }
            }
        }
        return false;
    }
    const char *valTag = strstr(message, "val=");
    if (valTag == nullptr) {
        valTag = strstr(message, "VAL=");
    }
    if (valTag == nullptr) {
        return false;
    }
    return userDataExtractBraceList(valTag + 4, outBraceList);
}

inline bool userDataMessageIsClusterReport(const char *message) {
    if (message == nullptr) {
        return false;
    }
    const char *clusterTag = strstr(message, "cl=0x");
    if (clusterTag == nullptr) {
        clusterTag = strstr(message, "cl=0X");
    }
    if (clusterTag == nullptr) {
        return false;
    }
    clusterTag += 5;
    char *endPointer = nullptr;
    const unsigned long clusterId = strtoul(clusterTag, &endPointer, 16);
    if (endPointer == clusterTag || clusterId != kUserDataClusterId) {
        return false;
    }
    const char *attrTag = strstr(message, "attr=");
    if (attrTag == nullptr) {
        attrTag = strstr(message, "ATTR=");
    }
    if (attrTag == nullptr) {
        return false;
    }
    attrTag += 5;
    while (*attrTag == ' ') {
        attrTag++;
    }
    if (strncmp(attrTag, "0x", 2) == 0 || strncmp(attrTag, "0X", 2) == 0) {
        attrTag += 2;
    }
    const unsigned long attributeId = strtoul(attrTag, &endPointer, 16);
    return endPointer != attrTag && attributeId == kUserDataAttributeId;
}

inline int userDataSplitBraceSlots(const String &braceList, String *slots, int maxSlots) {
    if (slots == nullptr || maxSlots <= 0) {
        return 0;
    }
    String body = braceList;
    body.trim();
    if (body.length() < 2 || body.charAt(0) != '{' || body.charAt(body.length() - 1) != '}') {
        return 0;
    }
    body = body.substring(1, body.length() - 1);
    int count = 0;
    String current;
    bool inString = false;
    bool escape = false;
    for (unsigned int index = 0; index < body.length(); index++) {
        const char character = body.charAt(index);
        if (inString) {
            current += character;
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
            current += character;
            continue;
        }
        if (character == ',') {
            if (count < maxSlots) {
                current.trim();
                slots[count++] = current;
            }
            current = "";
            continue;
        }
        current += character;
    }
    if (count < maxSlots) {
        current.trim();
        slots[count++] = current;
    }
    return count;
}

inline String userDataUnquoteSlot(const String &token) {
    String trimmed = token;
    trimmed.trim();
    if (trimmed.length() >= 2 && trimmed.charAt(0) == '"' && trimmed.charAt(trimmed.length() - 1) == '"') {
        return trimmed.substring(1, trimmed.length() - 1);
    }
    return trimmed;
}

inline String userDataQuoteSlot(const String &value) {
    String quoted = "\"";
    appendJsonEscaped(quoted, value.c_str(), kUserDataStringValueMaxChars);
    quoted += "\"";
    return quoted;
}

inline String userDataClampBraceListValues(const String &braceList) {
    String slots[kUserDataFieldMaxCount];
    const int slotCount = userDataSplitBraceSlots(braceList, slots, kUserDataFieldMaxCount);
    if (slotCount <= 0) {
        return braceList;
    }
    String clamped = "{";
    for (int index = 0; index < slotCount; index++) {
        if (index > 0) {
            clamped += ",";
        }
        String token = slots[index];
        token.trim();
        if (token.length() >= 2 && token.charAt(0) == '"' && token.charAt(token.length() - 1) == '"') {
            String value = userDataUnquoteSlot(token);
            userDataTruncateStringValue(&value);
            clamped += userDataQuoteSlot(value);
        } else {
            clamped += token;
        }
    }
    clamped += "}";
    return clamped;
}

inline String userDataSlotForField(
    const String &braceList,
    const UserDataFieldDef *fields,
    uint8_t fieldCount,
    const char *fieldName
) {
    if (fields == nullptr || fieldName == nullptr || fieldName[0] == '\0') {
        return String();
    }
    int fieldIndex = -1;
    for (uint8_t index = 0; index < fieldCount; index++) {
        if (strcmp(fields[index].name, fieldName) == 0) {
            fieldIndex = (int)index;
            break;
        }
    }
    if (fieldIndex < 0) {
        return String();
    }
    String slots[kUserDataFieldMaxCount];
    const int slotCount = userDataSplitBraceSlots(braceList, slots, kUserDataFieldMaxCount);
    if (fieldIndex >= slotCount) {
        return String();
    }
    if (userDataSlotIsNullToken(slots[fieldIndex])) {
        return String();
    }
    String value = userDataUnquoteSlot(slots[fieldIndex]);
    if (fields[fieldIndex].type == UserDataFieldTypeBool) {
        bool boolValue = false;
        if (userDataParseBoolToken(value, &boolValue)) {
            return boolValue ? String("ON") : String("OFF");
        }
    }
    if (fields[fieldIndex].type == UserDataFieldTypeString) {
        userDataTruncateStringValue(&value);
    }
    return value;
}
