#pragma once

#include "GlobalSettings.h"
#include "JsonField.h"

#include <Arduino.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

constexpr uint8_t kDeviceCustomCommandMaxCount = 8;
constexpr size_t kDeviceCustomCommandNameMaxChars = 11;
constexpr size_t kDeviceCustomCommandNameMaxLen = kDeviceCustomCommandNameMaxChars + 1;
constexpr size_t kDeviceCustomCommandValueMaxChars = 15;
constexpr size_t kDeviceCustomCommandValueMaxLen = kDeviceCustomCommandValueMaxChars + 1;

enum DeviceCustomCommandParamType : uint8_t {
    DeviceCustomCommandParamNone = 0,
    DeviceCustomCommandParamInt = 1,
    DeviceCustomCommandParamBool = 2,
    DeviceCustomCommandParamFloat = 3,
    DeviceCustomCommandParamString = 4
};

struct __attribute__((packed)) DeviceCustomCommandDef {
    uint8_t hasParam;
    uint8_t paramType;
    char name[kDeviceCustomCommandNameMaxLen];
};

inline DeviceCustomCommandParamType deviceCustomCommandParamTypeFromText(const char *text) {
    if (text == nullptr || text[0] == '\0') {
        return DeviceCustomCommandParamNone;
    }
    if (strcasecmp(text, "int") == 0) {
        return DeviceCustomCommandParamInt;
    }
    if (strcasecmp(text, "bool") == 0) {
        return DeviceCustomCommandParamBool;
    }
    if (strcasecmp(text, "float") == 0) {
        return DeviceCustomCommandParamFloat;
    }
    if (strcasecmp(text, "string") == 0) {
        return DeviceCustomCommandParamString;
    }
    return DeviceCustomCommandParamNone;
}

inline const char *deviceCustomCommandParamTypeJsonId(DeviceCustomCommandParamType type) {
    switch (type) {
        case DeviceCustomCommandParamInt:
            return "int";
        case DeviceCustomCommandParamBool:
            return "bool";
        case DeviceCustomCommandParamFloat:
            return "float";
        case DeviceCustomCommandParamString:
            return "string";
        default:
            return "";
    }
}

inline void deviceCustomCommandCopyName(char *destination, const char *source) {
    if (destination == nullptr) {
        return;
    }
    destination[0] = '\0';
    if (source == nullptr) {
        return;
    }
    size_t copyLen = 0;
    while (source[copyLen] != '\0' && copyLen < kDeviceCustomCommandNameMaxChars) {
        destination[copyLen] = source[copyLen];
        copyLen++;
    }
    destination[copyLen] = '\0';
}

inline void deviceCustomCommandClear(
    DeviceCustomCommandDef **commands,
    uint8_t *commandCount
) {
    if (commands != nullptr) {
        free(*commands);
        *commands = nullptr;
    }
    if (commandCount != nullptr) {
        *commandCount = 0;
    }
}

inline bool deviceCustomCommandCopy(
    DeviceCustomCommandDef **destinationCommands,
    uint8_t *destinationCount,
    const DeviceCustomCommandDef *sourceCommands,
    uint8_t sourceCount
) {
    if (destinationCommands == nullptr || destinationCount == nullptr) {
        return false;
    }
    deviceCustomCommandClear(destinationCommands, destinationCount);
    if (sourceCount == 0 || sourceCommands == nullptr) {
        return true;
    }
    if (sourceCount > kDeviceCustomCommandMaxCount) {
        sourceCount = kDeviceCustomCommandMaxCount;
    }
    DeviceCustomCommandDef *copy = (DeviceCustomCommandDef *)malloc(
        (size_t)sourceCount * sizeof(DeviceCustomCommandDef)
    );
    if (copy == nullptr) {
        return false;
    }
    memcpy(copy, sourceCommands, (size_t)sourceCount * sizeof(DeviceCustomCommandDef));
    *destinationCommands = copy;
    *destinationCount = sourceCount;
    return true;
}

inline const DeviceCustomCommandDef *deviceCustomCommandFindByName(
    const DeviceCustomCommandDef *commands,
    uint8_t commandCount,
    const char *name
) {
    if (commands == nullptr || name == nullptr || name[0] == '\0') {
        return nullptr;
    }
    for (uint8_t index = 0; index < commandCount; index++) {
        if (strcmp(commands[index].name, name) == 0) {
            return &commands[index];
        }
    }
    return nullptr;
}

inline bool deviceCustomCommandNameValid(const char *name) {
    if (name == nullptr || name[0] == '\0') {
        return false;
    }
    size_t length = 0;
    while (name[length] != '\0') {
        length++;
        if (length > kDeviceCustomCommandNameMaxChars) {
            return false;
        }
    }
    return true;
}

inline bool deviceCustomCommandDefsValid(
    const DeviceCustomCommandDef *commands,
    uint8_t commandCount
) {
    if (commandCount > kDeviceCustomCommandMaxCount) {
        return false;
    }
    if (commandCount == 0) {
        return true;
    }
    if (commands == nullptr) {
        return false;
    }
    for (uint8_t index = 0; index < commandCount; index++) {
        if (!deviceCustomCommandNameValid(commands[index].name)) {
            return false;
        }
        for (uint8_t prior = 0; prior < index; prior++) {
            if (strcmp(commands[prior].name, commands[index].name) == 0) {
                return false;
            }
        }
        if (commands[index].hasParam) {
            const DeviceCustomCommandParamType type =
                (DeviceCustomCommandParamType)commands[index].paramType;
            if (type != DeviceCustomCommandParamInt
                && type != DeviceCustomCommandParamBool
                && type != DeviceCustomCommandParamFloat
                && type != DeviceCustomCommandParamString) {
                return false;
            }
        } else if (commands[index].paramType != DeviceCustomCommandParamNone) {
            return false;
        }
    }
    return true;
}

inline const char *deviceCustomCommandFindJsonObjectEnd(const char *objectStart) {
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

inline uint8_t deviceCustomCommandParseJson(
    const char *json,
    DeviceCustomCommandDef *outCommands,
    uint8_t maxCount,
    bool *outValid
) {
    if (outValid != nullptr) {
        *outValid = true;
    }
    if (json == nullptr || outCommands == nullptr || maxCount == 0) {
        return 0;
    }
    uint8_t count = 0;
    const char *cursor = json;
    while (cursor != nullptr && *cursor != '\0') {
        const char *objectStart = strchr(cursor, '{');
        if (objectStart == nullptr) {
            break;
        }
        const char *objectEnd = deviceCustomCommandFindJsonObjectEnd(objectStart);
        if (objectEnd == nullptr) {
            if (outValid != nullptr) {
                *outValid = false;
            }
            return 0;
        }
        if (count >= maxCount) {
            if (outValid != nullptr) {
                *outValid = false;
            }
            return 0;
        }
        String object = String(objectStart).substring(0, (unsigned int)(objectEnd - objectStart + 1));
        String nameText;
        String typeText;
        bool hasParam = false;
        extractJsonString(object.c_str(), "name", nameText);
        extractJsonBool(object.c_str(), "hasParam", hasParam);
        extractJsonString(object.c_str(), "paramType", typeText);
        nameText.trim();
        DeviceCustomCommandDef &command = outCommands[count];
        memset(&command, 0, sizeof(command));
        deviceCustomCommandCopyName(command.name, nameText.c_str());
        command.hasParam = hasParam ? 1 : 0;
        if (command.hasParam) {
            command.paramType = (uint8_t)deviceCustomCommandParamTypeFromText(typeText.c_str());
            if (command.paramType == DeviceCustomCommandParamNone) {
                if (outValid != nullptr) {
                    *outValid = false;
                }
                return 0;
            }
        } else {
            command.paramType = DeviceCustomCommandParamNone;
        }
        count++;
        cursor = objectEnd + 1;
    }
    if (!deviceCustomCommandDefsValid(outCommands, count)) {
        if (outValid != nullptr) {
            *outValid = false;
        }
        return 0;
    }
    return count;
}

inline bool deviceCustomCommandAssignFromJson(
    DeviceCustomCommandDef **commands,
    uint8_t *commandCount,
    const char *json,
    bool *outValid
) {
    if (commands == nullptr || commandCount == nullptr) {
        return false;
    }
    DeviceCustomCommandDef parsed[kDeviceCustomCommandMaxCount];
    bool valid = true;
    const uint8_t count =
        deviceCustomCommandParseJson(json, parsed, kDeviceCustomCommandMaxCount, &valid);
    if (outValid != nullptr) {
        *outValid = valid;
    }
    if (!valid) {
        deviceCustomCommandClear(commands, commandCount);
        return false;
    }
    return deviceCustomCommandCopy(commands, commandCount, parsed, count);
}

inline String deviceCustomCommandsToJson(
    const DeviceCustomCommandDef *commands,
    uint8_t commandCount
) {
    String json = "[";
    if (commands == nullptr) {
        commandCount = 0;
    }
    for (uint8_t index = 0; index < commandCount; index++) {
        if (index > 0) {
            json += ",";
        }
        json += "{\"name\":\"";
        appendJsonEscaped(json, commands[index].name, kDeviceCustomCommandNameMaxChars);
        json += "\",\"hasParam\":";
        json += commands[index].hasParam ? "true" : "false";
        if (commands[index].hasParam) {
            json += ",\"paramType\":\"";
            json += deviceCustomCommandParamTypeJsonId(
                (DeviceCustomCommandParamType)commands[index].paramType
            );
            json += "\"";
        }
        json += "}";
    }
    json += "]";
    return json;
}

inline bool deviceCustomCommandEncodeValue(
    DeviceCustomCommandParamType type,
    const char *valueText,
    char *out,
    size_t outLen
) {
    if (out == nullptr || outLen < 2) {
        return false;
    }
    out[0] = '\0';
    if (type == DeviceCustomCommandParamNone) {
        return true;
    }
    if (valueText == nullptr) {
        return false;
    }
    String trimmed = String(valueText);
    trimmed.trim();
    if (trimmed.length() == 0) {
        return false;
    }
    switch (type) {
        case DeviceCustomCommandParamInt: {
            char *end = nullptr;
            long value = strtol(trimmed.c_str(), &end, 10);
            if (end == trimmed.c_str() || (end != nullptr && *end != '\0')) {
                return false;
            }
            snprintf(out, outLen, "%ld", value);
            return true;
        }
        case DeviceCustomCommandParamBool: {
            if (trimmed.equalsIgnoreCase("ON") || trimmed.equalsIgnoreCase("true")
                || trimmed == "1") {
                snprintf(out, outLen, "ON");
                return true;
            }
            if (trimmed.equalsIgnoreCase("OFF") || trimmed.equalsIgnoreCase("false")
                || trimmed == "0") {
                snprintf(out, outLen, "OFF");
                return true;
            }
            return false;
        }
        case DeviceCustomCommandParamFloat: {
            char *end = nullptr;
            double value = strtod(trimmed.c_str(), &end);
            if (end == trimmed.c_str() || (end != nullptr && *end != '\0')) {
                return false;
            }
            snprintf(out, outLen, "%.2f", value);
            return true;
        }
        case DeviceCustomCommandParamString: {
            if (trimmed.length() > kDeviceCustomCommandValueMaxChars) {
                return false;
            }
            snprintf(out, outLen, "%s", trimmed.c_str());
            return true;
        }
        default:
            return false;
    }
}

inline bool deviceCustomCommandBuildMqttPayload(
    const char *commandName,
    bool hasParam,
    DeviceCustomCommandParamType paramType,
    const char *valueText,
    char *out,
    size_t outLen
) {
    if (out == nullptr || outLen < 4 || !deviceCustomCommandNameValid(commandName)) {
        return false;
    }
    if (!hasParam) {
        snprintf(out, outLen, "%s", commandName);
        return true;
    }
    char encoded[kDeviceCustomCommandValueMaxLen + 8];
    if (!deviceCustomCommandEncodeValue(paramType, valueText, encoded, sizeof(encoded))) {
        return false;
    }
    snprintf(out, outLen, "%s=%s", commandName, encoded);
    return true;
}

inline bool deviceCustomCommandBuildZigbeeBody(
    const char *commandName,
    bool hasParam,
    DeviceCustomCommandParamType paramType,
    const char *valueText,
    char *out,
    size_t outLen
) {
    if (out == nullptr || outLen < 8 || !deviceCustomCommandNameValid(commandName)) {
        return false;
    }
    if (!hasParam) {
        snprintf(out, outLen, "cmd=%s", commandName);
        return true;
    }
    char encoded[kDeviceCustomCommandValueMaxLen + 8];
    if (!deviceCustomCommandEncodeValue(paramType, valueText, encoded, sizeof(encoded))) {
        return false;
    }
    snprintf(out, outLen, "cmd=%s,val=%s", commandName, encoded);
    return true;
}

inline bool deviceCustomCommandBuildPayloadForTransport(
    uint8_t transport,
    bool zigbeeMqttProtocol,
    const DeviceCustomCommandDef *command,
    const char *valueText,
    char *out,
    size_t outLen
) {
    if (command == nullptr) {
        return false;
    }
    const bool hasParam = command->hasParam != 0;
    const DeviceCustomCommandParamType paramType =
        (DeviceCustomCommandParamType)command->paramType;
    if (clampDeviceTransport(transport) == DeviceTransportMqtt) {
        if (zigbeeMqttProtocol) {
            return deviceCustomCommandBuildZigbeeBody(
                command->name,
                hasParam,
                paramType,
                valueText,
                out,
                outLen
            );
        }
        return deviceCustomCommandBuildMqttPayload(
            command->name,
            hasParam,
            paramType,
            valueText,
            out,
            outLen
        );
    }
    return deviceCustomCommandBuildZigbeeBody(
        command->name,
        hasParam,
        paramType,
        valueText,
        out,
        outLen
    );
}
