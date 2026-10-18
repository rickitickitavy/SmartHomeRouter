#pragma once

#include <Arduino.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

constexpr uint8_t kUserDataFieldMaxCount = 24;
constexpr size_t kUserDataFieldNameMaxLen = 32;
// Enough for 24 max-length name rows of persisted schema JSON.
constexpr size_t kUserDataFieldsJsonMaxLen = 2048;
constexpr uint16_t kUserDataClusterId = 0xFC00;
constexpr uint16_t kUserDataAttributeId = 0x0001;

enum UserDataFieldType : uint8_t {
    UserDataFieldTypeInt = 0,
    UserDataFieldTypeFloat = 1,
    UserDataFieldTypeBool = 2,
    UserDataFieldTypeString = 3
};

struct UserDataFieldDef {
    bool nullable;
    UserDataFieldType type;
    char name[kUserDataFieldNameMaxLen];
};

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
    return value;
}
