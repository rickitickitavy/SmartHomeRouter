#pragma once

#include <Arduino.h>
#include <string.h>

template<typename JsonBuf>
inline void appendJsonEscaped(JsonBuf &json, const char *value, size_t maxLength = 128) {
    if (value == nullptr) {
        return;
    }
    for (size_t index = 0; index < maxLength; index++) {
        const unsigned char character = (unsigned char)value[index];
        if (character == '\0' || character == 0xFF) {
            break;
        }
        if (character == '"' || character == '\\') {
            json += '\\';
            json += (char)character;
            continue;
        }
        // Keep UTF-8 bytes (>= 128). Drop only ASCII control characters.
        if (character < 32) {
            continue;
        }
        json += (char)character;
    }
}

// Find `"key"` only at object nesting depth 1 (root object's own fields).
// Prevents nested keys (e.g. devices[].spacer) from shadowing parent fields.
inline const char *findJsonKeyAtRootDepth(const char *json, const char *key) {
    if (json == nullptr || key == nullptr || key[0] == '\0') {
        return nullptr;
    }
    const size_t keyLength = strlen(key);
    int depth = 0;
    bool inString = false;
    bool escape = false;
    for (const char *scan = json; *scan != '\0'; scan++) {
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
            if (depth == 1) {
                if (strncmp(scan + 1, key, keyLength) == 0 && scan[1 + keyLength] == '"') {
                    const char *afterKey = scan + 1 + keyLength + 1;
                    while (*afterKey == ' ' || *afterKey == '\t' || *afterKey == '\n' || *afterKey == '\r') {
                        afterKey++;
                    }
                    if (*afterKey == ':') {
                        return scan;
                    }
                }
            }
            inString = true;
            continue;
        }
        if (character == '{' || character == '[') {
            depth++;
            continue;
        }
        if (character == '}' || character == ']') {
            depth--;
            if (depth < 0) {
                depth = 0;
            }
        }
    }
    return nullptr;
}

inline bool extractJsonString(const char *json, const char *key, String &out) {
    if (json == nullptr || key == nullptr || key[0] == '\0') {
        return false;
    }
    String pattern = String("\"") + key + "\"";
    const size_t patternLength = pattern.length();
    const char *searchFrom = json;
    while (searchFrom != nullptr && *searchFrom != '\0') {
        const char *found = strstr(searchFrom, pattern.c_str());
        if (found == nullptr) {
            return false;
        }
        const char *afterKey = found + patternLength;
        while (*afterKey == ' ' || *afterKey == '\t' || *afterKey == '\n' || *afterKey == '\r') {
            afterKey++;
        }
        // Require `"key":` so a string *value* equal to the key (e.g. "content":"text")
        // is not treated as the key — otherwise the next field's value is stolen.
        if (*afterKey != ':') {
            searchFrom = found + 1;
            continue;
        }
        const char *quote = strchr(afterKey, '"');
        if (quote == nullptr) {
            return false;
        }
        quote++;
        const char *end = strchr(quote, '"');
        if (end == nullptr) {
            return false;
        }
        out = String(quote).substring(0, (unsigned int)(end - quote));
        return true;
    }
    return false;
}

inline bool extractJsonStringRoot(const char *json, const char *key, String &out) {
    const char *found = findJsonKeyAtRootDepth(json, key);
    if (found == nullptr) {
        return false;
    }
    const char *colon = strchr(found, ':');
    if (colon == nullptr) {
        return false;
    }
    const char *quote = strchr(colon, '"');
    if (quote == nullptr) {
        return false;
    }
    quote++;
    const char *end = strchr(quote, '"');
    if (end == nullptr) {
        return false;
    }
    out = String(quote).substring(0, (unsigned int)(end - quote));
    return true;
}

inline bool extractJsonBool(const char *json, const char *key, bool &out) {
    String pattern = String("\"") + key + "\"";
    const char *found = strstr(json, pattern.c_str());
    if (found == nullptr) {
        return false;
    }
    const char *colon = strchr(found + pattern.length(), ':');
    if (colon == nullptr) {
        return false;
    }
    colon++;
    while (*colon == ' ' || *colon == '\t') {
        colon++;
    }
    if (strncmp(colon, "true", 4) == 0) {
        out = true;
        return true;
    }
    if (strncmp(colon, "false", 5) == 0) {
        out = false;
        return true;
    }
    return false;
}

inline bool extractJsonBoolRoot(const char *json, const char *key, bool &out) {
    const char *found = findJsonKeyAtRootDepth(json, key);
    if (found == nullptr) {
        return false;
    }
    const char *colon = strchr(found, ':');
    if (colon == nullptr) {
        return false;
    }
    colon++;
    while (*colon == ' ' || *colon == '\t' || *colon == '\n' || *colon == '\r') {
        colon++;
    }
    if (strncmp(colon, "true", 4) == 0) {
        out = true;
        return true;
    }
    if (strncmp(colon, "false", 5) == 0) {
        out = false;
        return true;
    }
    return false;
}

inline bool extractJsonInt(const char *json, const char *key, int &out) {
    String pattern = String("\"") + key + "\"";
    const char *found = strstr(json, pattern.c_str());
    if (found == nullptr) {
        return false;
    }
    const char *colon = strchr(found + pattern.length(), ':');
    if (colon == nullptr) {
        return false;
    }
    colon++;
    while (*colon == ' ' || *colon == '\t') {
        colon++;
    }
    if (*colon == '"') {
        colon++;
    }
    if (*colon != '-' && (*colon < '0' || *colon > '9')) {
        return false;
    }
    out = atoi(colon);
    return true;
}

inline bool extractJsonIntRoot(const char *json, const char *key, int &out) {
    const char *found = findJsonKeyAtRootDepth(json, key);
    if (found == nullptr) {
        return false;
    }
    const char *colon = strchr(found, ':');
    if (colon == nullptr) {
        return false;
    }
    colon++;
    while (*colon == ' ' || *colon == '\t' || *colon == '\n' || *colon == '\r') {
        colon++;
    }
    if (*colon == '"') {
        colon++;
    }
    if (*colon != '-' && (*colon < '0' || *colon > '9')) {
        return false;
    }
    out = atoi(colon);
    return true;
}

inline bool extractJsonDouble(const char *json, const char *key, double &out) {
    String pattern = String("\"") + key + "\"";
    const char *found = strstr(json, pattern.c_str());
    if (found == nullptr) {
        return false;
    }
    const char *colon = strchr(found + pattern.length(), ':');
    if (colon == nullptr) {
        return false;
    }
    colon++;
    while (*colon == ' ' || *colon == '\t') {
        colon++;
    }
    if (*colon == '"') {
        colon++;
    }
    if (*colon != '-' && *colon != '.' && (*colon < '0' || *colon > '9')) {
        return false;
    }
    out = atof(colon);
    return true;
}

inline bool extractJsonKeyedSlice(const char *json, const char *key, char openBrace, String &out) {
    if (json == nullptr || key == nullptr) {
        return false;
    }
    // Only the root object's own fields (depth 1). Nested keys such as
    // users[].consoles must not shadow the top-level "consoles" store array.
    const char *found = findJsonKeyAtRootDepth(json, key);
    if (found == nullptr) {
        return false;
    }
    const char *colon = strchr(found, ':');
    if (colon == nullptr) {
        return false;
    }
    colon++;
    while (*colon == ' ' || *colon == '\t' || *colon == '\n' || *colon == '\r') {
        colon++;
    }
    if (*colon != openBrace) {
        return false;
    }
    int depth = 0;
    bool inString = false;
    bool escape = false;
    for (const char *cursor = colon; *cursor != '\0'; cursor++) {
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
            if (depth == 0) {
                out = String(colon).substring(0, (unsigned int)(cursor - colon + 1));
                return true;
            }
        }
    }
    return false;
}
