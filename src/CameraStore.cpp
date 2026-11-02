#include "CameraStore.h"
#include "JsonField.h"
#include "Logger.h"
#include "OnvifClient.h"

#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <esp_heap_caps.h>
#include <esp_random.h>
#include <mbedtls/md5.h>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

namespace {

enum FieldState {
    FieldMissing = 0,
    FieldOk,
    FieldBad
};

bool isJsonSpace(char character) {
    return character == ' ' || character == '\t' || character == '\n' || character == '\r';
}

const char *skipSpace(const char *cursor, const char *end) {
    while (cursor < end && isJsonSpace(*cursor)) {
        cursor++;
    }
    return cursor;
}

// cursor at the opening quote; returns the position after the closing quote or nullptr.
const char *skipString(const char *cursor, const char *end) {
    if (cursor == nullptr || cursor >= end || *cursor != '"') {
        return nullptr;
    }
    cursor++;
    while (cursor < end) {
        if (*cursor == '\\') {
            cursor += 2;
            continue;
        }
        if (*cursor == '"') {
            return cursor + 1;
        }
        cursor++;
    }
    return nullptr;
}

// Returns the position right after the JSON value starting at cursor, or nullptr.
const char *skipValue(const char *cursor, const char *end) {
    cursor = skipSpace(cursor, end);
    if (cursor >= end) {
        return nullptr;
    }
    if (*cursor == '"') {
        return skipString(cursor, end);
    }
    if (*cursor == '{' || *cursor == '[') {
        int depth = 0;
        while (cursor < end) {
            const char character = *cursor;
            if (character == '"') {
                cursor = skipString(cursor, end);
                if (cursor == nullptr) {
                    return nullptr;
                }
                continue;
            }
            if (character == '{' || character == '[') {
                depth++;
            } else if (character == '}' || character == ']') {
                depth--;
                if (depth == 0) {
                    return cursor + 1;
                }
            }
            cursor++;
        }
        return nullptr;
    }
    const char *scan = cursor;
    while (scan < end && *scan != ',' && *scan != '}' && *scan != ']' && !isJsonSpace(*scan)) {
        scan++;
    }
    return scan > cursor ? scan : nullptr;
}

// Finds a member of the object spanning [begin, end). No allocation, depth-correct.
bool findMember(const char *begin, const char *end, const char *key, const char **valueOut) {
    const char *cursor = skipSpace(begin, end);
    if (cursor >= end || *cursor != '{') {
        return false;
    }
    cursor++;
    const size_t keyLength = strlen(key);
    while (true) {
        cursor = skipSpace(cursor, end);
        if (cursor < end && *cursor == ',') {
            cursor++;
            continue;
        }
        if (cursor >= end || *cursor != '"') {
            return false;
        }
        const char *keyStart = cursor + 1;
        const char *afterKey = skipString(cursor, end);
        if (afterKey == nullptr) {
            return false;
        }
        const size_t rawKeyLength = (size_t)((afterKey - 1) - keyStart);
        cursor = skipSpace(afterKey, end);
        if (cursor >= end || *cursor != ':') {
            return false;
        }
        cursor = skipSpace(cursor + 1, end);
        if (rawKeyLength == keyLength && memcmp(keyStart, key, keyLength) == 0) {
            *valueOut = cursor;
            return cursor < end;
        }
        cursor = skipValue(cursor, end);
        if (cursor == nullptr) {
            return false;
        }
    }
}

bool isNullLiteral(const char *value, const char *end) {
    return end - value >= 4 && strncmp(value, "null", 4) == 0;
}

bool parseHex4(const char *cursor, const char *end, uint32_t &codePoint) {
    if (end - cursor < 4) {
        return false;
    }
    uint32_t value = 0;
    for (int index = 0; index < 4; index++) {
        const char digit = cursor[index];
        value <<= 4;
        if (digit >= '0' && digit <= '9') {
            value |= (uint32_t)(digit - '0');
        } else if (digit >= 'a' && digit <= 'f') {
            value |= (uint32_t)(digit - 'a' + 10);
        } else if (digit >= 'A' && digit <= 'F') {
            value |= (uint32_t)(digit - 'A' + 10);
        } else {
            return false;
        }
    }
    codePoint = value;
    return true;
}

struct StringSink {
    char *out;
    size_t capacity;
    size_t length;
    bool overflow;

    void put(unsigned char byte) {
        if (byte < 32) {
            return;  // drop ASCII control characters
        }
        if (length + 1 >= capacity) {
            overflow = true;
            return;
        }
        out[length++] = (char)byte;
    }

    void putCodePoint(uint32_t codePoint) {
        if (codePoint < 0x80) {
            put((unsigned char)codePoint);
        } else if (codePoint < 0x800) {
            put((unsigned char)(0xC0 | (codePoint >> 6)));
            put((unsigned char)(0x80 | (codePoint & 0x3F)));
        } else if (codePoint < 0x10000) {
            put((unsigned char)(0xE0 | (codePoint >> 12)));
            put((unsigned char)(0x80 | ((codePoint >> 6) & 0x3F)));
            put((unsigned char)(0x80 | (codePoint & 0x3F)));
        } else {
            put((unsigned char)(0xF0 | (codePoint >> 18)));
            put((unsigned char)(0x80 | ((codePoint >> 12) & 0x3F)));
            put((unsigned char)(0x80 | ((codePoint >> 6) & 0x3F)));
            put((unsigned char)(0x80 | (codePoint & 0x3F)));
        }
    }
};

// Decodes the JSON string at value straiI port ght into out (no temporary heap String).
bool decodeJsonString(const char *value, const char *end, char *out, size_t outSize, bool &overflow) {
    overflow = false;
    if (out == nullptr || outSize == 0 || value == nullptr || value >= end || *value != '"') {
        return false;
    }
    StringSink sink = {out, outSize, 0, false};
    const char *cursor = value + 1;
    while (cursor < end) {
        const char character = *cursor++;
        if (character == '"') {
            out[sink.length] = '\0';
            overflow = sink.overflow;
            return true;
        }
        if (character != '\\') {
            sink.put((unsigned char)character);
            continue;
        }
        if (cursor >= end) {
            return false;
        }
        const char escaped = *cursor++;
        switch (escaped) {
            case 'n':
            case 'r':
            case 't':
            case 'b':
            case 'f':
                break;  // control characters are dropped
            case 'u': {
                uint32_t codePoint = 0;
                if (!parseHex4(cursor, end, codePoint)) {
                    return false;
                }
                cursor += 4;
                if (codePoint >= 0xD800 && codePoint <= 0xDBFF) {
                    uint32_t low = 0;
                    if (end - cursor >= 6 && cursor[0] == '\\' && cursor[1] == 'u'
                        && parseHex4(cursor + 2, end, low) && low >= 0xDC00 && low <= 0xDFFF) {
                        cursor += 6;
                        codePoint = 0x10000 + (((codePoint - 0xD800) << 10) | (low - 0xDC00));
                        sink.putCodePoint(codePoint);
                    } else {
                        sink.put('?');
                    }
                } else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF) {
                    sink.put('?');
                } else {
                    sink.putCodePoint(codePoint);
                }
                break;
            }
            default:
                sink.put((unsigned char)escaped);  // '"', '\\', '/', and anything else verbatim
                break;
        }
    }
    return false;
}

FieldState readStringField(const char *begin, const char *end, const char *key, char *out, size_t outSize) {
    const char *value = nullptr;
    if (!findMember(begin, end, key, &value)) {
        return FieldMissing;
    }
    if (isNullLiteral(value, end)) {
        out[0] = '\0';
        return FieldOk;
    }
    bool overflow = false;
    if (!decodeJsonString(value, end, out, outSize, overflow) || overflow) {
        return FieldBad;
    }
    return FieldOk;
}

FieldState readBoolField(const char *begin, const char *end, const char *key, bool &out) {
    const char *value = nullptr;
    if (!findMember(begin, end, key, &value)) {
        return FieldMissing;
    }
    if (end - value >= 4 && strncmp(value, "true", 4) == 0) {
        out = true;
        return FieldOk;
    }
    if (end - value >= 5 && strncmp(value, "false", 5) == 0) {
        out = false;
        return FieldOk;
    }
    return FieldBad;
}

// Accepts 123 or "123".
FieldState readUnsignedField(const char *begin, const char *end, const char *key, uint32_t &out) {
    const char *value = nullptr;
    if (!findMember(begin, end, key, &value)) {
        return FieldMissing;
    }
    const bool quoted = value < end && *value == '"';
    const char *cursor = quoted ? value + 1 : value;
    uint32_t number = 0;
    int digits = 0;
    while (cursor < end && *cursor >= '0' && *cursor <= '9') {
        number = number * 10 + (uint32_t)(*cursor - '0');
        if (number > 1000000UL) {
            return FieldBad;
        }
        digits++;
        cursor++;
    }
    if (digits == 0) {
        return FieldBad;
    }
    out = number;
    return FieldOk;
}

bool isHttpUrl(const char *url) {
    if (url == nullptr) {
        return false;
    }
    return strncasecmp(url, "http://", 7) == 0 || strncasecmp(url, "https://", 8) == 0;
}

bool isRtspUrl(const char *url) {
    if (url == nullptr) {
        return false;
    }
    return strncasecmp(url, "rtsp://", 7) == 0 || strncasecmp(url, "rtsps://", 8) == 0;
}

// Empty is allowed; non-empty must be an http(s) URL (display/snapshot).
bool isUrlFieldValid(const char *url) {
    return url == nullptr || url[0] == '\0' || isHttpUrl(url);
}

// Live URL may be browser HTTP(S) or ONVIF/RTSP stream URI.
bool isLiveUrlFieldValid(const char *url) {
    return url == nullptr || url[0] == '\0' || isHttpUrl(url) || isRtspUrl(url);
}

void secureZero(void *block, size_t length) {
    volatile unsigned char *bytes = (volatile unsigned char *)block;
    while (length-- > 0) {
        *bytes++ = 0;
    }
}

void copyBounded(char *dest, size_t destSize, const char *source) {
    if (destSize == 0) {
        return;
    }
    size_t length = source != nullptr ? strlen(source) : 0;
    if (length >= destSize) {
        length = destSize - 1;
    }
    if (length > 0) {
        memcpy(dest, source, length);
    }
    dest[length] = '\0';
}

// Extracts host/port from "http(s)://[user:pass@]host[:port]/path".
bool parseUrlHostPort(const char *url, char *hostOut, size_t hostSize, uint16_t &portOut) {
    if (!isHttpUrl(url) || hostSize == 0) {
        return false;
    }
    const bool secure = strncasecmp(url, "https://", 8) == 0;
    const char *cursor = url + (secure ? 8 : 7);
    portOut = secure ? 443 : 80;
    const char *authorityEnd = cursor;
    while (*authorityEnd != '\0' && *authorityEnd != '/' && *authorityEnd != '?' && *authorityEnd != '#') {
        authorityEnd++;
    }
    for (const char *scan = cursor; scan < authorityEnd; scan++) {
        if (*scan == '@') {
            cursor = scan + 1;
        }
    }
    const char *hostStart = cursor;
    const char *hostEnd = nullptr;
    const char *portStart = nullptr;
    if (*cursor == '[') {
        hostStart = cursor + 1;
        hostEnd = hostStart;
        while (hostEnd < authorityEnd && *hostEnd != ']') {
            hostEnd++;
        }
        if (hostEnd >= authorityEnd) {
            return false;
        }
        if (hostEnd + 1 < authorityEnd && hostEnd[1] == ':') {
            portStart = hostEnd + 2;
        }
    } else {
        hostEnd = hostStart;
        while (hostEnd < authorityEnd && *hostEnd != ':') {
            hostEnd++;
        }
        if (hostEnd < authorityEnd) {
            portStart = hostEnd + 1;
        }
    }
    const size_t hostLength = (size_t)(hostEnd - hostStart);
    if (hostLength == 0 || hostLength >= hostSize) {
        return false;
    }
    memcpy(hostOut, hostStart, hostLength);
    hostOut[hostLength] = '\0';
    if (portStart != nullptr && portStart < authorityEnd) {
        uint32_t port = 0;
        for (const char *scan = portStart; scan < authorityEnd; scan++) {
            if (*scan < '0' || *scan > '9') {
                return false;
            }
            port = port * 10 + (uint32_t)(*scan - '0');
            if (port > 65535) {
                return false;
            }
        }
        if (port == 0) {
            return false;
        }
        portOut = (uint16_t)port;
    }
    return true;
}

bool resolveProbeTarget(const CameraRecord *camera, char *hostOut, size_t hostSize, uint16_t &portOut) {
    if (camera->onvif.host[0] != '\0') {
        copyBounded(hostOut, hostSize, camera->onvif.host);
        portOut = camera->onvif.port != 0 ? camera->onvif.port : 80;
        return true;
    }
    if (parseUrlHostPort(camera->url, hostOut, hostSize, portOut)) {
        return true;
    }
    for (int channel = 0; channel < CAMERA_CHANNELS; channel++) {
        if (parseUrlHostPort(camera->onvif.channels[channel].snapshotUrl, hostOut, hostSize, portOut)) {
            return true;
        }
    }
    return false;
}

void setCameraError(CameraRecord *camera, const char *message) {
    camera->status = CAMERA_STATUS_ERROR;
    copyBounded(camera->statusError, sizeof(camera->statusError), message);
}

void md5Hex(const char *input, char outHex[33]) {
    if (outHex == nullptr) {
        return;
    }
    outHex[0] = '\0';
    if (input == nullptr) {
        return;
    }
    unsigned char digest[16];
    mbedtls_md5(
        reinterpret_cast<const unsigned char *>(input),
        strlen(input),
        digest
    );
    for (int index = 0; index < 16; index++) {
        sprintf(outHex + index * 2, "%02x", digest[index]);
    }
    outHex[32] = '\0';
}

bool extractAuthParam(const char *header, const char *name, char *out, size_t outMax) {
    if (header == nullptr || name == nullptr || out == nullptr || outMax == 0) {
        return false;
    }
    out[0] = '\0';
    const size_t nameLength = strlen(name);
    const char *cursor = header;
    while ((cursor = strcasestr(cursor, name)) != nullptr) {
        if (cursor > header) {
            const char before = cursor[-1];
            if (before != ' ' && before != ',' && before != '\t') {
                cursor += nameLength;
                continue;
            }
        }
        cursor += nameLength;
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
        if (*cursor != '=') {
            continue;
        }
        cursor++;
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
        if (*cursor == '"') {
            cursor++;
            const char *end = strchr(cursor, '"');
            if (end == nullptr) {
                return false;
            }
            const size_t length = (size_t)(end - cursor);
            if (length >= outMax) {
                return false;
            }
            memcpy(out, cursor, length);
            out[length] = '\0';
            return out[0] != '\0';
        }
        const char *end = cursor;
        while (*end != '\0' && *end != ',' && *end != ' ' && *end != '\t') {
            end++;
        }
        const size_t length = (size_t)(end - cursor);
        if (length >= outMax) {
            return false;
        }
        memcpy(out, cursor, length);
        out[length] = '\0';
        return out[0] != '\0';
    }
    return false;
}

bool urlRequestUri(const char *url, char *out, size_t outMax) {
    if (url == nullptr || out == nullptr || outMax == 0) {
        return false;
    }
    const char *path = url;
    if (strncmp(url, "http://", 7) == 0) {
        path = strchr(url + 7, '/');
    } else if (strncmp(url, "https://", 8) == 0) {
        path = strchr(url + 8, '/');
    }
    if (path == nullptr || path[0] == '\0') {
        copyBounded(out, outMax, "/");
        return true;
    }
    copyBounded(out, outMax, path);
    return out[0] != '\0';
}

bool buildDigestAuthorization(
    const char *username,
    const char *password,
    const char *method,
    const char *requestUri,
    const char *wwwAuthenticate,
    char *outHeader,
    size_t outMax
) {
    if (outHeader == nullptr || outMax == 0) {
        return false;
    }
    outHeader[0] = '\0';
    if (username == nullptr || username[0] == '\0' || method == nullptr || requestUri == nullptr
        || wwwAuthenticate == nullptr || strcasestr(wwwAuthenticate, "Digest") == nullptr) {
        return false;
    }
    char realm[96];
    char nonce[128];
    char opaque[128];
    char qop[32];
    realm[0] = '\0';
    nonce[0] = '\0';
    opaque[0] = '\0';
    qop[0] = '\0';
    if (!extractAuthParam(wwwAuthenticate, "realm", realm, sizeof(realm))
        || !extractAuthParam(wwwAuthenticate, "nonce", nonce, sizeof(nonce))) {
        return false;
    }
    extractAuthParam(wwwAuthenticate, "opaque", opaque, sizeof(opaque));
    extractAuthParam(wwwAuthenticate, "qop", qop, sizeof(qop));

    char ha1Input[256];
    snprintf(
        ha1Input,
        sizeof(ha1Input),
        "%s:%s:%s",
        username,
        realm,
        password != nullptr ? password : ""
    );
    char ha1[33];
    md5Hex(ha1Input, ha1);

    char ha2Input[320];
    snprintf(ha2Input, sizeof(ha2Input), "%s:%s", method, requestUri);
    char ha2[33];
    md5Hex(ha2Input, ha2);

    char response[33];
    const bool useQop = qop[0] != '\0' && strcasestr(qop, "auth") != nullptr;
    char cnonce[17];
    cnonce[0] = '\0';
    if (useQop) {
        unsigned char raw[8];
        esp_fill_random(raw, sizeof(raw));
        for (int index = 0; index < 8; index++) {
            sprintf(cnonce + index * 2, "%02x", raw[index]);
        }
        cnonce[16] = '\0';
        char responseInput[320];
        snprintf(
            responseInput,
            sizeof(responseInput),
            "%s:%s:00000001:%s:auth:%s",
            ha1,
            nonce,
            cnonce,
            ha2
        );
        md5Hex(responseInput, response);
    } else {
        char responseInput[256];
        snprintf(responseInput, sizeof(responseInput), "%s:%s:%s", ha1, nonce, ha2);
        md5Hex(responseInput, response);
    }

    int written = 0;
    if (useQop) {
        written = snprintf(
            outHeader,
            outMax,
            "Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", "
            "algorithm=MD5, response=\"%s\", qop=auth, nc=00000001, cnonce=\"%s\"",
            username,
            realm,
            nonce,
            requestUri,
            response,
            cnonce
        );
    } else {
        written = snprintf(
            outHeader,
            outMax,
            "Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", "
            "algorithm=MD5, response=\"%s\"",
            username,
            realm,
            nonce,
            requestUri,
            response
        );
    }
    if (written <= 0 || (size_t)written >= outMax) {
        outHeader[0] = '\0';
        return false;
    }
    if (opaque[0] != '\0') {
        const size_t used = strlen(outHeader);
        snprintf(outHeader + used, outMax - used, ", opaque=\"%s\"", opaque);
    }
    return true;
}

bool readHttpBodyToBuffer(
    HTTPClient &http,
    uint8_t *buffer,
    size_t bufferMax,
    size_t *outBytes,
    uint32_t timeoutMs
) {
    if (outBytes != nullptr) {
        *outBytes = 0;
    }
    if (buffer == nullptr || bufferMax == 0 || outBytes == nullptr) {
        return false;
    }
    WiFiClient *stream = http.getStreamPtr();
    if (stream == nullptr) {
        return false;
    }
    const int contentLength = http.getSize();
    size_t bytes = 0;
    const uint32_t deadlineMs = millis() + timeoutMs;
    const uint32_t firstByteDeadlineMs = millis() + CAMERA_SNAPSHOT_FIRST_BYTE_MS;
    while (bytes < bufferMax && millis() < deadlineMs) {
        if (contentLength > 0 && (int)bytes >= contentLength) {
            break;
        }
        const size_t available = stream->available();
        if (available == 0) {
            if (bytes == 0) {
                // Snapshot CGI often stalls before the first JPEG byte.
                if (millis() < firstByteDeadlineMs) {
                    delay(10);
                    yield();
                    continue;
                }
                if (!http.connected()) {
                    delay(80);
                    if (stream->available() == 0) {
                        break;
                    }
                    continue;
                }
                delay(10);
                yield();
                continue;
            }
            if (!http.connected() && stream->available() == 0) {
                delay(25);
                if (stream->available() == 0) {
                    break;
                }
                continue;
            }
            delay(5);
            yield();
            continue;
        }
        size_t toRead = available;
        if (toRead > bufferMax - bytes) {
            toRead = bufferMax - bytes;
        }
        const int readCount = stream->readBytes(reinterpret_cast<char *>(buffer + bytes), toRead);
        if (readCount <= 0) {
            break;
        }
        bytes += (size_t)readCount;
        yield();
    }
    *outBytes = bytes;
    return bytes > 0;
}

HTTPClient *createSnapshotHttpClient() {
    // Heap-allocate — HTTPClient embeds a large WiFiClient and blows task stacks.
    HTTPClient *http = new (std::nothrow) HTTPClient();
    if (http == nullptr) {
        return nullptr;
    }
    http->setReuse(false);
    http->setTimeout(12000);
    http->setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    return http;
}

bool beginSnapshotGet(HTTPClient *http, const char *url) {
    if (http == nullptr || url == nullptr || url[0] == '\0') {
        return false;
    }
    const char *headerKeys[] = {"WWW-Authenticate", "Content-Type"};
    http->collectHeaders(headerKeys, 2);
    if (!http->begin(url)) {
        return false;
    }
    http->addHeader("Connection", "close");
    http->addHeader("Accept", "image/jpeg, */*");
    return true;
}

bool finishSnapshotJpeg(
    HTTPClient *http,
    uint8_t *buffer,
    size_t bufferMax,
    size_t *outBytes,
    char *errorText,
    size_t errorMax,
    int status
) {
    auto setErr = [&](const char *message) {
        if (errorText != nullptr && errorMax > 0 && message != nullptr) {
            strncpy(errorText, message, errorMax - 1);
            errorText[errorMax - 1] = '\0';
        }
    };
    if (http == nullptr) {
        setErr("HTTP client");
        return false;
    }
    if (status <= 0) {
        char statusError[CAMERA_STATUS_ERROR_MAX];
        snprintf(statusError, sizeof(statusError), "HTTP connect failed (%d)", status);
        setErr(statusError);
        http->end();
        delete http;
        return false;
    }
    if (status != HTTP_CODE_OK) {
        char statusError[CAMERA_STATUS_ERROR_MAX];
        snprintf(statusError, sizeof(statusError), "HTTP %d", status);
        setErr(statusError);
        http->end();
        delete http;
        return false;
    }
    size_t bytes = 0;
    const int contentLength = http->getSize();
    if (!readHttpBodyToBuffer(*http, buffer, bufferMax, &bytes, 12000) || bytes < 2) {
        LOGGER.warning(
            String("Camera snapshot: empty body status=")
            + String(status)
            + " contentLength="
            + String(contentLength)
        );
        // Many ONVIF cams advertise snapshot.cgi but reply 200 with Content-Length 0.
        if (contentLength == 0) {
            setErr("0-byte JPEG from camera");
        } else {
            setErr("empty body");
        }
        http->end();
        delete http;
        return false;
    }
    http->end();
    delete http;
    if (!(buffer[0] == 0xFF && buffer[1] == 0xD8)) {
        setErr("not JPEG");
        if (outBytes != nullptr) {
            *outBytes = bytes;
        }
        return false;
    }
    if (outBytes != nullptr) {
        *outBytes = bytes;
    }
    if (errorText != nullptr && errorMax > 0) {
        errorText[0] = '\0';
    }
    return true;
}

// GET snapshot URL into PSRAM. Probe without Basic first (many cams answer Basic with
// HTTP 200 + empty body instead of 401). Then Digest or Basic from WWW-Authenticate.
bool httpFetchSnapshotBody(
    const char *url,
    const char *username,
    const char *password,
    uint8_t *buffer,
    size_t bufferMax,
    size_t *outBytes,
    char *errorText,
    size_t errorMax
) {
    if (outBytes != nullptr) {
        *outBytes = 0;
    }
    if (errorText != nullptr && errorMax > 0) {
        errorText[0] = '\0';
    }
    if (url == nullptr || url[0] == '\0' || buffer == nullptr || bufferMax == 0 || outBytes == nullptr) {
        if (errorText != nullptr && errorMax > 0) {
            strncpy(errorText, "bad args", errorMax - 1);
        }
        return false;
    }

    char requestUri[CAMERA_URL_MAX];
    if (!urlRequestUri(url, requestUri, sizeof(requestUri))) {
        if (errorText != nullptr && errorMax > 0) {
            strncpy(errorText, "bad URL", errorMax - 1);
        }
        return false;
    }

    auto setErr = [&](const char *message) {
        if (errorText != nullptr && errorMax > 0 && message != nullptr) {
            strncpy(errorText, message, errorMax - 1);
            errorText[errorMax - 1] = '\0';
        }
    };

    const bool haveUser = username != nullptr && username[0] != '\0';

    for (int attempt = 0; attempt < CAMERA_SNAPSHOT_ATTEMPTS; attempt++) {
        if (attempt > 0) {
            delay((uint32_t)CAMERA_SNAPSHOT_RETRY_MS * (uint32_t)attempt);
            yield();
        }

        HTTPClient *probe = createSnapshotHttpClient();
        if (probe == nullptr) {
            setErr("out of memory");
            return false;
        }
        if (!beginSnapshotGet(probe, url)) {
            delete probe;
            setErr("bad URL");
            return false;
        }

        // Never send Basic on the first request — some cameras return 200 empty.
        int status = probe->GET();
        if (status == 401 && haveUser) {
            const String challenge = probe->header("WWW-Authenticate");
            probe->end();
            delete probe;
            probe = nullptr;

            if (challenge.length() > 0 && strcasestr(challenge.c_str(), "Digest") != nullptr) {
                char digestHeader[512];
                if (!buildDigestAuthorization(
                        username,
                        password,
                        "GET",
                        requestUri,
                        challenge.c_str(),
                        digestHeader,
                        sizeof(digestHeader)
                    )) {
                    setErr("HTTP 401");
                    continue;
                }
                HTTPClient *digestHttp = createSnapshotHttpClient();
                if (digestHttp == nullptr) {
                    setErr("out of memory");
                    return false;
                }
                if (!beginSnapshotGet(digestHttp, url)) {
                    delete digestHttp;
                    setErr("bad URL");
                    return false;
                }
                digestHttp->addHeader("Authorization", digestHeader);
                status = digestHttp->GET();
                if (finishSnapshotJpeg(
                        digestHttp, buffer, bufferMax, outBytes, errorText, errorMax, status
                    )) {
                    return true;
                }
                continue;
            }

            // Basic-only challenge (or missing Digest).
            HTTPClient *basicHttp = createSnapshotHttpClient();
            if (basicHttp == nullptr) {
                setErr("out of memory");
                return false;
            }
            if (!beginSnapshotGet(basicHttp, url)) {
                delete basicHttp;
                setErr("bad URL");
                return false;
            }
            basicHttp->setAuthorization(username, password != nullptr ? password : "");
            status = basicHttp->GET();
            if (finishSnapshotJpeg(
                    basicHttp, buffer, bufferMax, outBytes, errorText, errorMax, status
                )) {
                return true;
            }
            continue;
        }

        if (finishSnapshotJpeg(probe, buffer, bufferMax, outBytes, errorText, errorMax, status)) {
            return true;
        }
        // finishSnapshotJpeg already ended+deleted probe on all paths.
    }
    if (errorText != nullptr && errorMax > 0 && errorText[0] == '\0') {
        setErr("empty body");
    }
    return false;
}

}  // namespace

CameraStore::CameraStore()
    : head(nullptr),
      storedCount(0),
      persistEnabled(false),
      persistPending(false),
      discoveredHead(nullptr),
      discoveredStoredCount(0),
      discovering(false),
      discoveryUdpPhase(false),
      discoveryHttpPhase(false),
      discoveryType(CAMERA_TYPE_ONVIF),
      discoveryStartedMs(0),
      discoveryLastProbeMs(0),
      discoveryProbeRound(0),
      discoveryUdpOpen(false),
      discoveryCommand(0),
      discoveryCommandType(CAMERA_TYPE_ONVIF),
      discoveryScanOctet(1),
      discoveryScanBase(0, 0, 0, 0),
      discoveryScanSelfOctet(0),
      discoveryHttpPortIndex(0) {
}

CameraStore::~CameraStore() {
    discoveryCommand = 0;
    endDiscoveryUdp();
    discoveryHttpPhase = false;
    discovering = false;
    clearDiscovered();
    clearAll();
}

// ---------------------------------------------------------------------------
// PSRAM-only allocation
// ---------------------------------------------------------------------------

bool CameraStore::psramAllowsAllocation(size_t bytes) {
    if (bytes == 0) {
        return true;
    }
    const size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const size_t maxBlock = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    return freePsram >= bytes && maxBlock >= bytes;
}

void *CameraStore::allocatePsram(size_t bytes) {
    if (bytes == 0 || !psramAllowsAllocation(bytes)) {
        return nullptr;
    }
    void *block = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (block == nullptr) {
        block = ps_malloc(bytes);
    }
    // Never fall back to internal malloc(): camera data is PSRAM-only.
    return block;
}

CameraRecord *CameraStore::allocateRecord() {
    CameraRecord *camera = (CameraRecord *)allocatePsram(sizeof(CameraRecord));
    if (camera == nullptr) {
        return nullptr;
    }
    memset(camera, 0, sizeof(*camera));
    camera->active = true;
    camera->type = CAMERA_TYPE_ONVIF;
    camera->onvif.port = 80;
    return camera;
}

void CameraStore::freeCamera(CameraRecord *camera) {
    if (camera == nullptr) {
        return;
    }
    secureZero(camera->onvif.password, sizeof(camera->onvif.password));
    free(camera);
}

void CameraStore::freeCameraList(CameraRecord *&listHead) {
    while (listHead != nullptr) {
        CameraRecord *next = listHead->next;
        freeCamera(listHead);
        listHead = next;
    }
    listHead = nullptr;
}

void CameraStore::clearAll() {
    freeCameraList(head);
    storedCount = 0;
}

void CameraStore::freeDiscoveredList(DiscoveredCamera *&listHead) {
    while (listHead != nullptr) {
        DiscoveredCamera *next = listHead->next;
        free(listHead);
        listHead = next;
    }
    listHead = nullptr;
}

void CameraStore::clearDiscovered() {
    freeDiscoveredList(discoveredHead);
    discoveredStoredCount = 0;
}

DiscoveredCamera *CameraStore::allocateDiscovered() {
    void *block = allocatePsram(sizeof(DiscoveredCamera));
    if (block == nullptr) {
        return nullptr;
    }
    DiscoveredCamera *entry = static_cast<DiscoveredCamera *>(block);
    memset(entry, 0, sizeof(*entry));
    entry->port = 80;
    entry->type = CAMERA_TYPE_ONVIF;
    return entry;
}

// ---------------------------------------------------------------------------
// Basic access
// ---------------------------------------------------------------------------

int CameraStore::cameraCount() const {
    return storedCount;
}

CameraRecord *CameraStore::first() {
    return head;
}

const CameraRecord *CameraStore::first() const {
    return head;
}

CameraRecord *CameraStore::findById(const char *cameraId) {
    return const_cast<CameraRecord *>(static_cast<const CameraStore *>(this)->findById(cameraId));
}

const CameraRecord *CameraStore::findById(const char *cameraId) const {
    if (cameraId == nullptr || cameraId[0] == '\0') {
        return nullptr;
    }
    for (const CameraRecord *camera = head; camera != nullptr; camera = camera->next) {
        if (strcmp(camera->id, cameraId) == 0) {
            return camera;
        }
    }
    return nullptr;
}

CameraRecord *CameraStore::findTail() const {
    CameraRecord *tail = head;
    while (tail != nullptr && tail->next != nullptr) {
        tail = tail->next;
    }
    return tail;
}

void CameraStore::appendRecord(CameraRecord *camera) {
    camera->next = nullptr;
    CameraRecord *tail = findTail();
    if (tail == nullptr) {
        head = camera;
    } else {
        tail->next = camera;
    }
    storedCount++;
}

bool CameraStore::idInList(const CameraRecord *listHead, const char *cameraId, const CameraRecord *skip) {
    for (const CameraRecord *camera = listHead; camera != nullptr; camera = camera->next) {
        if (camera != skip && strcmp(camera->id, cameraId) == 0) {
            return true;
        }
    }
    return false;
}

bool CameraStore::isIdSafe(const char *cameraId) {
    if (cameraId == nullptr || cameraId[0] == '\0' || strlen(cameraId) >= CAMERA_ID_BUF) {
        return false;
    }
    for (const char *cursor = cameraId; *cursor != '\0'; cursor++) {
        const char character = *cursor;
        if ((character >= '0' && character <= '9') || (character >= 'a' && character <= 'z')
            || (character >= 'A' && character <= 'Z') || character == '-' || character == '_') {
            continue;
        }
        return false;
    }
    return true;
}

void CameraStore::makeCameraId(char *outId) {
    if (outId == nullptr) {
        return;
    }
    snprintf(outId, CAMERA_ID_BUF, "cam-%08x", (unsigned int)(esp_random() ^ (uint32_t)millis()));
}

const char *CameraStore::writeResultText(CameraWriteResult result) {
    switch (result) {
        case CameraWriteOk:
            return "ok";
        case CameraWriteNotFound:
            return "not found";
        case CameraWriteFull:
            return "store full";
        case CameraWriteBadId:
            return "bad id";
        case CameraWriteBadName:
            return "bad name";
        case CameraWriteDuplicateId:
            return "duplicate id";
        case CameraWriteValidation:
            return "validation failed";
        case CameraWriteOutOfMemory:
            return "out of PSRAM";
        case CameraWriteParseError:
            return "parse error";
        default:
            return "unknown error";
    }
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

bool CameraStore::begin(bool persistToLittleFs) {
    persistEnabled = persistToLittleFs;
    if (!persistEnabled) {
        return true;
    }
    if (loadFromFile()) {
        LOGGER.info("Loaded " + String(storedCount) + " camera(s)");
        return true;
    }
    clearAll();
    return true;
}

void CameraStore::requestPersist() {
    persistPending = true;
}

void CameraStore::persistIfDue() {
    if (!persistPending) {
        return;
    }
    if (!persistEnabled) {
        persistPending = false;
        return;
    }
    persistPending = false;
    if (!saveToFile()) {
        LOGGER.error("Camera store write failed");
        persistPending = true;
        return;
    }
    LOGGER.info("Camera store saved " + String(storedCount) + " camera(s)");
}

bool CameraStore::saveToFile() const {
    PsramJsonBuffer json;
    json += "[";
    bool firstItem = true;
    for (const CameraRecord *camera = head; camera != nullptr; camera = camera->next) {
        if (!firstItem) {
            json += ",";
        }
        firstItem = false;
        appendCameraFullJson(json, camera, true, false);  // runtime status is never persisted
    }
    json += "]";
    if (!json.ok()) {
        return false;
    }
    File file = LittleFS.open(CAMERAS_STORE_PATH, "w");
    if (!file) {
        LOGGER.error("Cameras file write failed");
        return false;
    }
    const bool written = json.writeToFile(file);
    file.close();
    return written;
}

bool CameraStore::loadFromFile() {
    File file = LittleFS.open(CAMERAS_STORE_PATH, "r");
    if (!file) {
        return false;
    }
    PsramJsonBuffer json;
    const bool loaded = json.assignFromFile(file);
    file.close();
    if (!loaded || !json.ok()) {
        return false;
    }
    json.trim();
    if (json.length() == 0 || json.charAt(0) != '[') {
        return false;
    }
    CameraRecord *newHead = nullptr;
    int count = 0;
    const CameraWriteResult result = parseArray(json.c_str(), &newHead, &count);
    if (result != CameraWriteOk) {
        LOGGER.error(String("Cameras file load failed: ") + writeResultText(result));
        return false;
    }
    clearAll();
    head = newHead;
    storedCount = count;
    return true;
}

// ---------------------------------------------------------------------------
// JSON parse (allocation-free scanner; strings decode straight into PSRAM records)
// ---------------------------------------------------------------------------

CameraWriteResult CameraStore::fillRecord(CameraRecord *camera, const char *begin, const char *end,
    const CameraRecord *previous, bool requireId) {
    FieldState state = readStringField(begin, end, "id", camera->id, sizeof(camera->id));
    if (state == FieldBad) {
        return CameraWriteBadId;
    }
    if (camera->id[0] != '\0' && !isIdSafe(camera->id)) {
        return CameraWriteBadId;
    }
    if (requireId && camera->id[0] == '\0') {
        return CameraWriteBadId;
    }

    state = readStringField(begin, end, "name", camera->name, sizeof(camera->name));
    if (state != FieldOk || camera->name[0] == '\0') {
        return CameraWriteBadName;
    }

    bool activeValue = true;
    if (readBoolField(begin, end, "active", activeValue) == FieldBad) {
        return CameraWriteValidation;
    }
    camera->active = activeValue;

    // Type is a fixed enum: only "onvif" (or numeric 0) is accepted.
    const char *typeValue = nullptr;
    if (findMember(begin, end, "type", &typeValue)) {
        char typeText[16];
        bool overflow = false;
        if (typeValue < end && *typeValue == '"') {
            if (!decodeJsonString(typeValue, end, typeText, sizeof(typeText), overflow) || overflow
                || strcmp(typeText, "onvif") != 0) {
                return CameraWriteValidation;
            }
        } else {
            uint32_t typeNumber = 0;
            if (readUnsignedField(begin, end, "type", typeNumber) != FieldOk || typeNumber != CAMERA_TYPE_ONVIF) {
                return CameraWriteValidation;
            }
        }
    }
    camera->type = CAMERA_TYPE_ONVIF;

    if (readStringField(begin, end, "url", camera->url, sizeof(camera->url)) == FieldBad
        || !isUrlFieldValid(camera->url)) {
        return CameraWriteValidation;
    }
    if (readStringField(begin, end, "host", camera->onvif.host, sizeof(camera->onvif.host)) == FieldBad) {
        return CameraWriteValidation;
    }
    if (readStringField(begin, end, "username", camera->onvif.username, sizeof(camera->onvif.username))
        == FieldBad) {
        return CameraWriteValidation;
    }
    state = readStringField(begin, end, "password", camera->onvif.password, sizeof(camera->onvif.password));
    if (state == FieldBad) {
        return CameraWriteValidation;
    }
    if (state == FieldMissing && previous != nullptr) {
        copyBounded(camera->onvif.password, sizeof(camera->onvif.password), previous->onvif.password);
    }

    uint32_t portValue = 80;
    state = readUnsignedField(begin, end, "port", portValue);
    if (state == FieldBad || portValue == 0 || portValue > 65535) {
        return CameraWriteValidation;
    }
    camera->onvif.port = (uint16_t)portValue;

    // Legacy "users" arrays in saved JSON are ignored (per-camera ACL removed).

    // channels: [{"snapshotUrl":"..","liveUrl":".."}, {...}]  (at most CAMERA_CHANNELS)
    const char *channelsValue = nullptr;
    if (findMember(begin, end, "channels", &channelsValue) && !isNullLiteral(channelsValue, end)) {
        if (channelsValue >= end || *channelsValue != '[') {
            return CameraWriteValidation;
        }
        const char *cursor = channelsValue + 1;
        int channelIndex = 0;
        while (true) {
            cursor = skipSpace(cursor, end);
            if (cursor < end && *cursor == ',') {
                cursor++;
                continue;
            }
            if (cursor >= end) {
                return CameraWriteParseError;
            }
            if (*cursor == ']') {
                break;
            }
            if (*cursor != '{') {
                return CameraWriteValidation;
            }
            const char *objectEnd = skipValue(cursor, end);
            if (objectEnd == nullptr) {
                return CameraWriteParseError;
            }
            if (channelIndex >= CAMERA_CHANNELS) {
                return CameraWriteValidation;
            }
            CameraChannel &channel = camera->onvif.channels[channelIndex];
            if (readStringField(cursor, objectEnd, "snapshotUrl", channel.snapshotUrl, sizeof(channel.snapshotUrl))
                    == FieldBad
                || readStringField(cursor, objectEnd, "liveUrl", channel.liveUrl, sizeof(channel.liveUrl))
                    == FieldBad
                || !isUrlFieldValid(channel.snapshotUrl) || !isLiveUrlFieldValid(channel.liveUrl)) {
                return CameraWriteValidation;
            }
            channelIndex++;
            cursor = objectEnd;
        }
    }

    applyDefaultStatus(camera);
    return CameraWriteOk;
}

CameraWriteResult CameraStore::parseObject(const char *begin, const char *end, const CameraRecord *previous,
    bool requireId, CameraRecord **outCamera) {
    *outCamera = nullptr;
    const char *objectStart = skipSpace(begin, end);
    if (objectStart >= end || *objectStart != '{') {
        return CameraWriteParseError;
    }
    CameraRecord *camera = allocateRecord();
    if (camera == nullptr) {
        return CameraWriteOutOfMemory;
    }
    const CameraWriteResult result = fillRecord(camera, objectStart, end, previous, requireId);
    if (result != CameraWriteOk) {
        freeCamera(camera);
        return result;
    }
    *outCamera = camera;
    return CameraWriteOk;
}

CameraWriteResult CameraStore::parseArray(const char *arrayJson, CameraRecord **outHead, int *outCount) {
    *outHead = nullptr;
    *outCount = 0;
    if (arrayJson == nullptr) {
        return CameraWriteParseError;
    }
    const char *end = arrayJson + strlen(arrayJson);
    const char *cursor = skipSpace(arrayJson, end);
    if (cursor >= end || *cursor != '[') {
        return CameraWriteParseError;
    }
    cursor++;
    CameraRecord *newHead = nullptr;
    CameraRecord *tail = nullptr;
    int count = 0;
    CameraWriteResult result = CameraWriteOk;
    while (true) {
        cursor = skipSpace(cursor, end);
        if (cursor < end && *cursor == ',') {
            cursor++;
            continue;
        }
        if (cursor >= end) {
            result = CameraWriteParseError;
            break;
        }
        if (*cursor == ']') {
            break;
        }
        if (*cursor != '{') {
            result = CameraWriteParseError;
            break;
        }
        const char *objectEnd = skipValue(cursor, end);
        if (objectEnd == nullptr) {
            result = CameraWriteParseError;
            break;
        }
        if (count >= CAMERA_MAX_COUNT) {
            result = CameraWriteFull;
            break;
        }
        CameraRecord *camera = nullptr;
        result = parseObject(cursor, objectEnd, nullptr, false, &camera);
        if (result != CameraWriteOk) {
            break;
        }
        if (camera->id[0] == '\0') {
            do {
                makeCameraId(camera->id);
            } while (idInList(newHead, camera->id, nullptr));
        } else if (idInList(newHead, camera->id, nullptr)) {
            freeCamera(camera);
            result = CameraWriteDuplicateId;
            break;
        }
        camera->next = nullptr;
        if (tail == nullptr) {
            newHead = camera;
        } else {
            tail->next = camera;
        }
        tail = camera;
        count++;
        cursor = objectEnd;
    }
    if (result != CameraWriteOk) {
        freeCameraList(newHead);
        return result;
    }
    *outHead = newHead;
    *outCount = count;
    return CameraWriteOk;
}

// ---------------------------------------------------------------------------
// CRUD
// ---------------------------------------------------------------------------

CameraWriteResult CameraStore::createFromJson(const char *json, CameraRecord **createdOut) {
    if (createdOut != nullptr) {
        *createdOut = nullptr;
    }
    if (json == nullptr) {
        return CameraWriteParseError;
    }
    if (storedCount >= CAMERA_MAX_COUNT) {
        return CameraWriteFull;
    }
    CameraRecord *camera = nullptr;
    const CameraWriteResult result = parseObject(json, json + strlen(json), nullptr, false, &camera);
    if (result != CameraWriteOk) {
        return result;
    }
    if (camera->id[0] == '\0') {
        do {
            makeCameraId(camera->id);
        } while (findById(camera->id) != nullptr);
    } else if (findById(camera->id) != nullptr) {
        freeCamera(camera);
        return CameraWriteDuplicateId;
    }
    appendRecord(camera);
    if (persistEnabled) {
        requestPersist();
    }
    if (createdOut != nullptr) {
        *createdOut = camera;
    }
    return CameraWriteOk;
}

CameraWriteResult CameraStore::replaceFromJson(const char *cameraId, const char *json, bool keepId) {
    if (cameraId == nullptr || cameraId[0] == '\0') {
        return CameraWriteBadId;
    }
    if (json == nullptr) {
        return CameraWriteParseError;
    }
    CameraRecord *existing = findById(cameraId);
    if (existing == nullptr) {
        return CameraWriteNotFound;
    }
    CameraRecord *parsed = nullptr;
    const CameraWriteResult result = parseObject(json, json + strlen(json), existing, false, &parsed);
    if (result != CameraWriteOk) {
        return result;
    }
    if (keepId || parsed->id[0] == '\0') {
        copyBounded(parsed->id, sizeof(parsed->id), existing->id);
    } else if (idInList(head, parsed->id, existing)) {
        freeCamera(parsed);
        return CameraWriteDuplicateId;
    }
    // Swap the parsed record into the existing slot.
    parsed->next = existing->next;
    if (head == existing) {
        head = parsed;
    } else {
        CameraRecord *previous = head;
        while (previous != nullptr && previous->next != existing) {
            previous = previous->next;
        }
        if (previous == nullptr) {
            freeCamera(parsed);
            return CameraWriteNotFound;
        }
        previous->next = parsed;
    }
    freeCamera(existing);
    if (persistEnabled) {
        requestPersist();
    }
    return CameraWriteOk;
}

CameraWriteResult CameraStore::deleteById(const char *cameraId) {
    if (cameraId == nullptr || cameraId[0] == '\0') {
        return CameraWriteBadId;
    }
    CameraRecord *previous = nullptr;
    CameraRecord *current = head;
    while (current != nullptr) {
        if (strcmp(current->id, cameraId) == 0) {
            if (previous == nullptr) {
                head = current->next;
            } else {
                previous->next = current->next;
            }
            freeCamera(current);
            storedCount--;
            if (persistEnabled) {
                requestPersist();
            }
            return CameraWriteOk;
        }
        previous = current;
        current = current->next;
    }
    return CameraWriteNotFound;
}

CameraWriteResult CameraStore::replaceAllFromJson(const String &arrayJson) {
    return replaceAllFromJson(arrayJson.c_str());
}

CameraWriteResult CameraStore::replaceAllFromJson(const char *arrayJson) {
    CameraRecord *newHead = nullptr;
    int count = 0;
    const CameraWriteResult result = parseArray(arrayJson, &newHead, &count);
    if (result != CameraWriteOk) {
        return result;
    }
    clearAll();
    head = newHead;
    storedCount = count;
    if (persistEnabled) {
        requestPersist();
    }
    return CameraWriteOk;
}

// ---------------------------------------------------------------------------
// JSON output
// ---------------------------------------------------------------------------

void CameraStore::applyDefaultStatus(CameraRecord *camera) {
    camera->probed = false;
    camera->lastProbeMs = 0;
    camera->statusError[0] = '\0';
    if (!camera->active) {
        camera->status = CAMERA_STATUS_OFFLINE;
        return;
    }
    setCameraError(camera, "not checked");
}

const char *CameraStore::statusText(const CameraRecord *camera) {
    if (camera == nullptr || !camera->active || camera->status == CAMERA_STATUS_OFFLINE) {
        return "offline";
    }
    if (camera->status == CAMERA_STATUS_ONLINE) {
        return "online";
    }
    return camera->statusError[0] != '\0' ? camera->statusError : "error";
}

void CameraStore::appendCameraListItemJson(PsramJsonBuffer &json, const CameraRecord *camera) {
    if (camera == nullptr) {
        return;
    }
    json += "{\"id\":\"";
    appendJsonEscaped(json, camera->id, CAMERA_ID_BUF);
    json += "\",\"name\":\"";
    appendJsonEscaped(json, camera->name, CAMERA_NAME_MAX);
    json += "\",\"active\":";
    json += camera->active ? "true" : "false";
    json += ",\"type\":\"onvif\",\"url\":\"";
    appendJsonEscaped(json, camera->url, CAMERA_URL_MAX);
    json += "\",\"status\":\"";
    appendJsonEscaped(json, statusText(camera), CAMERA_STATUS_ERROR_MAX);
    json += "\"}";
}

void CameraStore::appendCameraFullJson(PsramJsonBuffer &json, const CameraRecord *camera, bool includePassword,
    bool includeStatus) {
    if (camera == nullptr) {
        return;
    }
    json += "{\"id\":\"";
    appendJsonEscaped(json, camera->id, CAMERA_ID_BUF);
    json += "\",\"name\":\"";
    appendJsonEscaped(json, camera->name, CAMERA_NAME_MAX);
    json += "\",\"active\":";
    json += camera->active ? "true" : "false";
    json += ",\"type\":\"onvif\",\"url\":\"";
    appendJsonEscaped(json, camera->url, CAMERA_URL_MAX);
    json += "\",\"host\":\"";
    appendJsonEscaped(json, camera->onvif.host, CAMERA_HOST_MAX);
    json += "\",\"port\":";
    json += String((unsigned)camera->onvif.port);
    json += ",\"username\":\"";
    appendJsonEscaped(json, camera->onvif.username, CAMERA_USER_MAX);
    json += "\"";
    if (includePassword) {
        json += ",\"password\":\"";
        appendJsonEscaped(json, camera->onvif.password, CAMERA_PASSWORD_MAX);
        json += "\"";
    }
    json += ",\"channels\":[";
    for (int channelIndex = 0; channelIndex < CAMERA_CHANNELS; channelIndex++) {
        if (channelIndex > 0) {
            json += ",";
        }
        json += "{\"snapshotUrl\":\"";
        appendJsonEscaped(json, camera->onvif.channels[channelIndex].snapshotUrl, CAMERA_URL_MAX);
        json += "\",\"liveUrl\":\"";
        appendJsonEscaped(json, camera->onvif.channels[channelIndex].liveUrl, CAMERA_URL_MAX);
        json += "\"}";
    }
    json += "]";
    if (includeStatus) {
        json += ",\"status\":\"";
        appendJsonEscaped(json, statusText(camera), CAMERA_STATUS_ERROR_MAX);
        json += "\"";
    }
    json += "}";
}

void CameraStore::appendListJson(PsramJsonBuffer &json, const CameraStore &store, bool includeSecrets) {
    json += "[";
    bool firstItem = true;
    for (const CameraRecord *camera = store.head; camera != nullptr; camera = camera->next) {
        if (!firstItem) {
            json += ",";
        }
        firstItem = false;
        if (includeSecrets) {
            appendCameraFullJson(json, camera, true, true);
        } else {
            appendCameraListItemJson(json, camera);
        }
    }
    json += "]";
}

String CameraStore::listJson(bool includeSecrets) const {
    PsramJsonBuffer json;
    appendListJson(json, *this, includeSecrets);
    if (!json.ok()) {
        return String();
    }
    return json.toString();
}

String CameraStore::cameraFullJson(const CameraRecord *camera, bool includePassword) const {
    if (camera == nullptr) {
        return String();
    }
    PsramJsonBuffer json;
    appendCameraFullJson(json, camera, includePassword, true);
    if (!json.ok()) {
        return String();
    }
    return json.toString();
}

// ---------------------------------------------------------------------------
// Access control and status
// ---------------------------------------------------------------------------

bool CameraStore::userMayView(const CameraRecord *camera, const char *userName, bool isAdminOrEditConsoles) {
    (void)userName;
    (void)isAdminOrEditConsoles;
    return camera != nullptr;
}

void CameraStore::probeStatus(CameraRecord *camera) {
    if (camera == nullptr) {
        return;
    }
    camera->probed = true;
    camera->lastProbeMs = millis();
    if (!camera->active) {
        camera->status = CAMERA_STATUS_OFFLINE;
        camera->statusError[0] = '\0';
        return;
    }
    char host[CAMERA_HOST_MAX];
    uint16_t port = 80;
    if (!resolveProbeTarget(camera, host, sizeof(host), port)) {
        setCameraError(camera, "no host");
        return;
    }
    // TCP connect only: no request is sent and no body is read, so nothing is buffered.
    WiFiClient client;
    if (!client.connect(host, port, (int32_t)CAMERA_PROBE_TIMEOUT_MS)) {
        setCameraError(camera, "unreachable");
        return;
    }
    client.stop();
    camera->status = CAMERA_STATUS_ONLINE;
    camera->statusError[0] = '\0';
}

void CameraStore::refreshAllStatus() {
    for (CameraRecord *camera = head; camera != nullptr; camera = camera->next) {
        probeStatus(camera);
        yield();
    }
}

// ---------------------------------------------------------------------------
// ONVIF WS-Discovery
// ---------------------------------------------------------------------------

bool CameraStore::parseHttpUrl(const char *url, char *hostOut, size_t hostMax, uint16_t *portOut) {
    if (url == nullptr || hostOut == nullptr || hostMax == 0 || portOut == nullptr) {
        return false;
    }
    hostOut[0] = '\0';
    *portOut = 80;
    const char *cursor = url;
    if (strncmp(cursor, "https://", 8) == 0) {
        cursor += 8;
        *portOut = 443;
    } else if (strncmp(cursor, "http://", 7) == 0) {
        cursor += 7;
        *portOut = 80;
    } else {
        return false;
    }
    size_t hostLength = 0;
    while (cursor[hostLength] != '\0' && cursor[hostLength] != '/' && cursor[hostLength] != ':'
        && cursor[hostLength] != '?' && cursor[hostLength] != ' ') {
        hostLength++;
    }
    if (hostLength == 0 || hostLength >= hostMax) {
        return false;
    }
    memcpy(hostOut, cursor, hostLength);
    hostOut[hostLength] = '\0';
    if (cursor[hostLength] == ':') {
        unsigned long parsedPort = strtoul(cursor + hostLength + 1, nullptr, 10);
        if (parsedPort >= 1UL && parsedPort <= 65535UL) {
            *portOut = (uint16_t)parsedPort;
        }
    }
    return true;
}

bool CameraStore::isHostRegistered(const char *host) const {
    if (host == nullptr || host[0] == '\0') {
        return false;
    }
    for (const CameraRecord *camera = head; camera != nullptr; camera = camera->next) {
        if (camera->onvif.host[0] != '\0' && strcasecmp(camera->onvif.host, host) == 0) {
            return true;
        }
        char urlHost[CAMERA_HOST_MAX];
        uint16_t urlPort = 80;
        if (parseHttpUrl(camera->url, urlHost, sizeof(urlHost), &urlPort)
            && strcasecmp(urlHost, host) == 0) {
            return true;
        }
        for (int channelIndex = 0; channelIndex < CAMERA_CHANNELS; channelIndex++) {
            if (parseHttpUrl(camera->onvif.channels[channelIndex].snapshotUrl, urlHost, sizeof(urlHost), &urlPort)
                && strcasecmp(urlHost, host) == 0) {
                return true;
            }
            if (parseHttpUrl(camera->onvif.channels[channelIndex].liveUrl, urlHost, sizeof(urlHost), &urlPort)
                && strcasecmp(urlHost, host) == 0) {
                return true;
            }
        }
    }
    return false;
}

bool CameraStore::hasDiscoveredHost(const char *host) const {
    if (host == nullptr || host[0] == '\0') {
        return false;
    }
    for (const DiscoveredCamera *entry = discoveredHead; entry != nullptr; entry = entry->next) {
        if (strcasecmp(entry->host, host) == 0) {
            return true;
        }
    }
    return false;
}

bool CameraStore::sendDiscoveryProbe(uint8_t probeKind) {
    char messageId[48];
    snprintf(
        messageId,
        sizeof(messageId),
        "uuid:%08x-%04x-%04x-%04x-%08x%04x",
        (unsigned)esp_random(),
        (unsigned)(esp_random() & 0xffff),
        (unsigned)(esp_random() & 0xffff),
        (unsigned)(esp_random() & 0xffff),
        (unsigned)esp_random(),
        (unsigned)(esp_random() & 0xffff)
    );
    // probeKind: 0 = NetworkVideoTransmitter, 1 = Device, 2 = untyped (match any).
    const char *typesXml = "";
    if (probeKind == 0) {
        typesXml = "<d:Types>dn:NetworkVideoTransmitter</d:Types>";
    } else if (probeKind == 1) {
        typesXml = "<d:Types>dn:Device</d:Types>";
    }
    char probe[900];
    const int written = snprintf(
        probe,
        sizeof(probe),
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<e:Envelope xmlns:e=\"http://www.w3.org/2003/05/soap-envelope\""
        " xmlns:w=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\""
        " xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\""
        " xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\">"
        "<e:Header>"
        "<w:MessageID>%s</w:MessageID>"
        "<w:To e:mustUnderstand=\"true\">urn:schemas-xmlsoap-org:ws:2005:04:discovery</w:To>"
        "<w:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</w:Action>"
        "</e:Header>"
        "<e:Body><d:Probe>%s</d:Probe></e:Body>"
        "</e:Envelope>",
        messageId,
        typesXml
    );
    if (written <= 0 || written >= (int)sizeof(probe)) {
        return false;
    }
    IPAddress multicastAddress(239, 255, 255, 250);
    if (!discoveryUdp.beginPacket(multicastAddress, 3702)) {
        return false;
    }
    discoveryUdp.write(reinterpret_cast<const uint8_t *>(probe), (size_t)written);
    return discoveryUdp.endPacket() == 1;
}

void CameraStore::sendDiscoveryProbeRound() {
    // Keep this short — long delay loops starve slave SPI and race WiFiUDP.
    drainDiscoveryUdp();
    bool anySent = sendDiscoveryProbe(0);
    delay(5);
    yield();
    drainDiscoveryUdp();
    anySent = sendDiscoveryProbe(1) || anySent;
    delay(5);
    yield();
    drainDiscoveryUdp();
    anySent = sendDiscoveryProbe(2) || anySent;
    discoveryLastProbeMs = millis();
    if (anySent) {
        discoveryProbeRound++;
    }
    drainDiscoveryUdp();
}

void CameraStore::drainDiscoveryUdp() {
    if (!discoveryUdpOpen) {
        return;
    }
    // Never use available()/byte-read loops after parsePacket — NetworkUDP can null the
    // RX cbuf between calls (LoadProhibited). One parsePacket → one bulk read.
    char discard[64];
    for (int drained = 0; drained < CAMERA_DISCOVER_DRAIN_MAX; drained++) {
        const int packetSize = discoveryUdp.parsePacket();
        if (packetSize <= 0) {
            break;
        }
        if (packetSize > 4096) {
            int left = packetSize;
            while (left > 0) {
                const int chunk = left > (int)sizeof(discard) ? (int)sizeof(discard) : left;
                const int got = discoveryUdp.read(discard, (size_t)chunk);
                if (got <= 0) {
                    break;
                }
                left -= got;
            }
            continue;
        }
        char *buffer = static_cast<char *>(allocatePsram((size_t)packetSize + 1));
        if (buffer == nullptr) {
            int left = packetSize;
            while (left > 0) {
                const int chunk = left > (int)sizeof(discard) ? (int)sizeof(discard) : left;
                const int got = discoveryUdp.read(discard, (size_t)chunk);
                if (got <= 0) {
                    break;
                }
                left -= got;
            }
            continue;
        }
        const int readCount = discoveryUdp.read(buffer, (size_t)packetSize);
        if (readCount > 0) {
            ingestDiscoveryPacket(buffer, (size_t)readCount);
        }
        free(buffer);
        yield();
    }
}

void CameraStore::ingestDiscoveryPacket(const char *payload, size_t length) {
    if (payload == nullptr || length == 0 || discoveredStoredCount >= CAMERA_DISCOVER_MAX) {
        return;
    }
    // Copy into a NUL-terminated PSRAM buffer for simple string searches.
    char *packet = static_cast<char *>(allocatePsram(length + 1));
    if (packet == nullptr) {
        return;
    }
    memcpy(packet, payload, length);
    packet[length] = '\0';

    const char *xaddrsTag = strstr(packet, "XAddrs");
    if (xaddrsTag == nullptr) {
        free(packet);
        return;
    }
    const char *xaddrsStart = strchr(xaddrsTag, '>');
    if (xaddrsStart == nullptr) {
        free(packet);
        return;
    }
    xaddrsStart++;
    const char *xaddrsEnd = strstr(xaddrsStart, "</");
    if (xaddrsEnd == nullptr || xaddrsEnd <= xaddrsStart) {
        free(packet);
        return;
    }

    // XAddrs may list several URLs (IPv6 first is common). Prefer first IPv4 http(s).
    char xaddr[CAMERA_URL_MAX];
    xaddr[0] = '\0';
    char host[CAMERA_HOST_MAX];
    host[0] = '\0';
    uint16_t port = 80;
    const char *cursor = xaddrsStart;
    while (cursor < xaddrsEnd) {
        while (cursor < xaddrsEnd
            && (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')) {
            cursor++;
        }
        if (cursor >= xaddrsEnd) {
            break;
        }
        const char *tokenStart = cursor;
        while (cursor < xaddrsEnd && *cursor != ' ' && *cursor != '\t' && *cursor != '\r'
            && *cursor != '\n') {
            cursor++;
        }
        char candidate[CAMERA_URL_MAX];
        const size_t tokenLength = (size_t)(cursor - tokenStart);
        if (tokenLength == 0 || tokenLength >= sizeof(candidate)) {
            continue;
        }
        memcpy(candidate, tokenStart, tokenLength);
        candidate[tokenLength] = '\0';
        // Skip IPv6 literals ([...]) — parseHttpUrl only handles IPv4 host forms.
        if (strstr(candidate, "://[") != nullptr) {
            continue;
        }
        char candidateHost[CAMERA_HOST_MAX];
        uint16_t candidatePort = 80;
        if (!parseHttpUrl(candidate, candidateHost, sizeof(candidateHost), &candidatePort)) {
            continue;
        }
        strncpy(xaddr, candidate, sizeof(xaddr) - 1);
        xaddr[sizeof(xaddr) - 1] = '\0';
        strncpy(host, candidateHost, sizeof(host) - 1);
        host[sizeof(host) - 1] = '\0';
        port = candidatePort;
        break;
    }
    if (xaddr[0] == '\0' || host[0] == '\0') {
        free(packet);
        return;
    }
    char name[CAMERA_NAME_MAX];
    name[0] = '\0';
    // Scopes use either name=Value or .../name/Value (Vatilon H80 uses the latter).
    const char *nameKey = strstr(packet, "name=");
    if (nameKey != nullptr) {
        nameKey += 5;
    } else {
        nameKey = strstr(packet, "/name/");
        if (nameKey != nullptr) {
            nameKey += 6;
        }
    }
    if (nameKey != nullptr) {
        size_t nameLength = 0;
        while (nameKey[nameLength] != '\0' && nameKey[nameLength] != ' ' && nameKey[nameLength] != '"'
            && nameKey[nameLength] != '<' && nameKey[nameLength] != '&' && nameKey[nameLength] != '/'
            && nameLength + 1 < sizeof(name)) {
            name[nameLength] = nameKey[nameLength];
            nameLength++;
        }
        name[nameLength] = '\0';
    }
    addDiscoveredCamera(name[0] != '\0' ? name : host, host, port, xaddr);
    free(packet);
}

bool CameraStore::addDiscoveredCamera(
    const char *name,
    const char *host,
    uint16_t port,
    const char *xaddr
) {
    if (host == nullptr || host[0] == '\0' || xaddr == nullptr || xaddr[0] == '\0') {
        return false;
    }
    if (discoveredStoredCount >= CAMERA_DISCOVER_MAX) {
        return false;
    }
    if (isHostRegistered(host) || hasDiscoveredHost(host)) {
        return false;
    }
    DiscoveredCamera *entry = allocateDiscovered();
    if (entry == nullptr) {
        LOGGER.warning(String("Camera discover: out of PSRAM adding ") + host);
        return false;
    }
    const char *useName = (name != nullptr && name[0] != '\0') ? name : host;
    strncpy(entry->name, useName, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    strncpy(entry->host, host, sizeof(entry->host) - 1);
    entry->host[sizeof(entry->host) - 1] = '\0';
    strncpy(entry->xaddr, xaddr, sizeof(entry->xaddr) - 1);
    entry->xaddr[sizeof(entry->xaddr) - 1] = '\0';
    strncpy(entry->url, xaddr, sizeof(entry->url) - 1);
    entry->url[sizeof(entry->url) - 1] = '\0';
    entry->port = port != 0 ? port : 80;
    entry->type = discoveryType;
    entry->next = discoveredHead;
    discoveredHead = entry;
    discoveredStoredCount++;
    LOGGER.info(
        String("Camera discover: found ") + entry->name + " @ " + entry->host + ":"
        + String((unsigned)entry->port) + " xaddr=" + entry->xaddr + " (total "
        + String(discoveredStoredCount) + ")"
    );
    return true;
}

bool CameraStore::startDiscovery(uint8_t cameraType) {
    if (cameraType != CAMERA_TYPE_ONVIF) {
        LOGGER.warning("Camera discover: unsupported type");
        return false;
    }
    // Queue only — WiFiUDP must be opened on the host loop (tickDiscovery).
    discoveryCommandType = cameraType;
    discoveryCommand = 1;
    discovering = true;
    return true;
}

void CameraStore::stopDiscovery() {
    discoveryCommand = 2;
}

void CameraStore::clearDiscoveryResults() {
    discoveryCommand = 3;
}

bool CameraStore::isDiscovering() const {
    return discovering || discoveryCommand == 1;
}

bool CameraStore::beginDiscoveryUdp() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    if (!discoveryUdp.beginMulticast(IPAddress(239, 255, 255, 250), 3702)) {
#else
    if (!discoveryUdp.beginMulticast(WiFi.localIP(), IPAddress(239, 255, 255, 250), 3702)) {
#endif
        LOGGER.warning("Camera discover: multicast begin failed");
        return false;
    }
    discoveryUdpOpen = true;
    return true;
}

void CameraStore::endDiscoveryUdp() {
    if (discoveryUdpOpen) {
        discoveryUdp.stop();
        discoveryUdpOpen = false;
    }
    discoveryUdpPhase = false;
}

void CameraStore::beginDiscoveryHttpScan() {
    discoveryHttpPhase = true;
    discoveryHttpPortIndex = 0;
    discoveryScanOctet = 1;
    IPAddress base(0, 0, 0, 0);
    uint8_t selfOctet = 0;
    const wifi_mode_t mode = WiFi.getMode();
    if (mode == WIFI_AP || mode == WIFI_AP_STA) {
        base = WiFi.softAPIP();
        selfOctet = base[3];
    }
    if ((uint32_t)base == 0 && (mode == WIFI_STA || mode == WIFI_AP_STA)) {
        if (WiFi.status() == WL_CONNECTED) {
            base = WiFi.localIP();
            selfOctet = base[3];
        }
    }
    discoveryScanBase = IPAddress(base[0], base[1], base[2], 0);
    discoveryScanSelfOctet = selfOctet;
    if ((uint32_t)discoveryScanBase == 0) {
        LOGGER.warning("Camera discover: HTTP scan skipped (no local IP)");
        discoveryHttpPhase = false;
        return;
    }
    LOGGER.info(
        String("Camera discover: HTTP ONVIF sweep ")
        + String(discoveryScanBase[0]) + "." + String(discoveryScanBase[1]) + "."
        + String(discoveryScanBase[2]) + ".1-254 (after WS-Discovery)"
    );
}

bool CameraStore::probeOnvifHttpHost(const char *host, uint16_t port, char *xaddrOut, size_t xaddrMax) {
    if (host == nullptr || host[0] == '\0' || xaddrOut == nullptr || xaddrMax == 0) {
        return false;
    }
    xaddrOut[0] = '\0';
    IPAddress ip;
    if (!ip.fromString(host)) {
        return false;
    }
    WiFiClient client;
    client.setTimeout(CAMERA_DISCOVER_HTTP_TIMEOUT_MS);
    // Timed connect — unused IPs must fail fast or slave SPI starves.
    if (!client.connect(ip, port, CAMERA_DISCOVER_HTTP_TIMEOUT_MS)) {
        return false;
    }
    static const char kBody[] =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"
        "<s:Body><tds:GetSystemDateAndTime xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\"/>"
        "</s:Body></s:Envelope>";
    client.print("POST /onvif/device_service HTTP/1.0\r\n");
    client.print("Host: ");
    client.print(host);
    if (port != 80) {
        client.print(":");
        client.print(port);
    }
    client.print("\r\nContent-Type: application/soap+xml; charset=utf-8\r\n");
    client.print("Content-Length: ");
    client.print(sizeof(kBody) - 1);
    client.print("\r\nConnection: close\r\n\r\n");
    client.print(kBody);

    // Must see real ONVIF/SOAP — routers/mesh APIs often answer HTTP 200 on :80/:8080.
    const uint32_t deadlineMs = millis() + (uint32_t)CAMERA_DISCOVER_HTTP_TIMEOUT_MS + 250UL;
    char reply[768];
    size_t replyLen = 0;
    reply[0] = '\0';
    while (millis() < deadlineMs && replyLen + 1 < sizeof(reply)) {
        while (client.available() > 0 && replyLen + 1 < sizeof(reply)) {
            reply[replyLen++] = (char)client.read();
            reply[replyLen] = '\0';
        }
        if (!client.connected() && client.available() == 0) {
            break;
        }
        delay(1);
        yield();
    }
    client.stop();
    if (replyLen == 0 || strstr(reply, "HTTP/") == nullptr) {
        return false;
    }
    if (strstr(reply, "404") != nullptr) {
        return false;
    }
    // Reject HTML login pages (mesh/routers).
    if (strcasestr(reply, "<html") != nullptr || strcasestr(reply, "<!doctype") != nullptr) {
        return false;
    }
    const bool looksOnvif = strcasestr(reply, "onvif") != nullptr
        || strstr(reply, "tds:") != nullptr
        || strstr(reply, "tt:") != nullptr
        || strcasestr(reply, "GetSystemDateAndTime") != nullptr
        || strcasestr(reply, "SOAP-ENV") != nullptr
        || strcasestr(reply, "soap:Envelope") != nullptr
        || strcasestr(reply, "Envelope") != nullptr;
    const bool soapContentType = strcasestr(reply, "application/soap") != nullptr
        || strcasestr(reply, "application/xml") != nullptr
        || strcasestr(reply, "text/xml") != nullptr;
    if (!looksOnvif && !soapContentType) {
        return false;
    }
    // XML/SOAP content-type alone is not enough without ONVIF/SOAP markers (or Envelope).
    if (!looksOnvif) {
        return false;
    }
    snprintf(xaddrOut, xaddrMax, "http://%s:%u/onvif/device_service", host, (unsigned)port);
    return true;
}

void CameraStore::tickDiscoveryHttpScan() {
    if (!discoveryHttpPhase) {
        return;
    }
    static const uint16_t kPorts[CAMERA_DISCOVER_HTTP_PORTS_MAX] = {80, 8080};
    // One host:port attempt per tick so slave SPI / web stay responsive.
    while (discoveryScanOctet < 255) {
        const uint8_t octet = discoveryScanOctet;
        if (octet == 0 || octet == discoveryScanSelfOctet) {
            discoveryScanOctet++;
            discoveryHttpPortIndex = 0;
            continue;
        }
        if (discoveryHttpPortIndex >= CAMERA_DISCOVER_HTTP_PORTS_MAX) {
            discoveryScanOctet++;
            discoveryHttpPortIndex = 0;
            continue;
        }
        const uint16_t port = kPorts[discoveryHttpPortIndex];
        discoveryHttpPortIndex++;
        char host[CAMERA_HOST_MAX];
        snprintf(
            host,
            sizeof(host),
            "%u.%u.%u.%u",
            (unsigned)discoveryScanBase[0],
            (unsigned)discoveryScanBase[1],
            (unsigned)discoveryScanBase[2],
            (unsigned)octet
        );
        if (isHostRegistered(host) || hasDiscoveredHost(host)) {
            discoveryScanOctet++;
            discoveryHttpPortIndex = 0;
            continue;
        }
        char xaddr[CAMERA_URL_MAX];
        if (probeOnvifHttpHost(host, port, xaddr, sizeof(xaddr))) {
            addDiscoveredCamera(host, host, port, xaddr);
        }
        return;
    }
    discoveryHttpPhase = false;
    discovering = false;
    LOGGER.info(
        String("Camera discover: done; found ") + String(discoveredStoredCount)
        + " unknown camera(s)"
    );
}

void CameraStore::applyDiscoveryCommand() {
    const uint8_t command = discoveryCommand;
    if (command == 0) {
        return;
    }
    discoveryCommand = 0;

    if (command == 1) {
        // Restart cleanly on the loop task.
        endDiscoveryUdp();
        discoveryHttpPhase = false;
        clearDiscovered();
        discoveryType = discoveryCommandType;
        discovering = true;
        discoveryUdpPhase = true;
        discoveryStartedMs = millis();
        discoveryLastProbeMs = 0;
        discoveryProbeRound = 0;
        if (!beginDiscoveryUdp()) {
            discovering = false;
            discoveryUdpPhase = false;
            return;
        }
        sendDiscoveryProbeRound();
        if (discoveryProbeRound == 0) {
            endDiscoveryUdp();
            discovering = false;
            LOGGER.warning("Camera discover: probe send failed");
            return;
        }
        LOGGER.info(
            String("Camera discover: ONVIF WS-Discovery started (window ")
            + String((unsigned)(CAMERA_DISCOVER_WINDOW_MS / 1000UL))
            + "s, then HTTP sweep)"
        );
        return;
    }

    if (command == 2 || command == 3) {
        const bool wasDiscovering = discovering || discoveryUdpPhase || discoveryHttpPhase;
        const int foundCount = discoveredStoredCount;
        const uint8_t rounds = discoveryProbeRound;
        endDiscoveryUdp();
        discoveryHttpPhase = false;
        discovering = false;
        discoveryLastProbeMs = 0;
        discoveryProbeRound = 0;
        if (command == 3) {
            const int previousCount = discoveredStoredCount;
            clearDiscovered();
            if (previousCount > 0) {
                LOGGER.info(
                    String("Camera discover: cleared ") + String(previousCount) + " result(s)"
                );
            }
        } else if (wasDiscovering) {
            LOGGER.info(
                String("Camera discover: stopped after ") + String((unsigned)rounds)
                + " probe round(s); found " + String(foundCount) + " unknown camera(s)"
            );
        }
    }
}

void CameraStore::tickDiscovery() {
    applyDiscoveryCommand();
    if (!discovering && !discoveryUdpPhase && !discoveryHttpPhase) {
        return;
    }
    if (discoveryUdpPhase) {
        drainDiscoveryUdp();
        const uint32_t elapsedMs = millis() - discoveryStartedMs;
        if ((millis() - discoveryLastProbeMs) >= CAMERA_DISCOVER_PROBE_INTERVAL_MS
            && elapsedMs + 400 < CAMERA_DISCOVER_WINDOW_MS) {
            sendDiscoveryProbeRound();
            drainDiscoveryUdp();
        }
        if (elapsedMs >= CAMERA_DISCOVER_WINDOW_MS) {
            drainDiscoveryUdp();
            endDiscoveryUdp();
            beginDiscoveryHttpScan();
            if (!discoveryHttpPhase) {
                discovering = false;
                LOGGER.info(
                    String("Camera discover: done; found ") + String(discoveredStoredCount)
                    + " unknown camera(s)"
                );
            }
        }
        return;
    }
    if (discoveryHttpPhase) {
        tickDiscoveryHttpScan();
    }
}

int CameraStore::discoveredCount() const {
    return discoveredStoredCount;
}

String CameraStore::discoveredJson() const {
    PsramJsonBuffer json;
    json += "[";
    bool first = true;
    for (const DiscoveredCamera *entry = discoveredHead; entry != nullptr; entry = entry->next) {
        if (isHostRegistered(entry->host)) {
            continue;
        }
        if (!first) {
            json += ",";
        }
        first = false;
        json += "{\"name\":\"";
        appendJsonEscaped(json, entry->name, CAMERA_NAME_MAX);
        json += "\",\"type\":\"onvif\",\"host\":\"";
        appendJsonEscaped(json, entry->host, CAMERA_HOST_MAX);
        json += "\",\"port\":";
        json += String((unsigned)entry->port);
        json += ",\"url\":\"";
        appendJsonEscaped(json, entry->url[0] != '\0' ? entry->url : entry->xaddr, CAMERA_URL_MAX);
        json += "\",\"xaddr\":\"";
        appendJsonEscaped(json, entry->xaddr, CAMERA_URL_MAX);
        json += "\"}";
    }
    json += "]";
    if (!json.ok()) {
        return String();
    }
    return json.toString();
}

String CameraStore::testOnvifJson(const char *jsonBody) const {
    String hostText;
    String urlText;
    String usernameText;
    String passwordText;
    int portValue = 80;
    if (jsonBody != nullptr) {
        extractJsonString(jsonBody, "host", hostText);
        extractJsonString(jsonBody, "url", urlText);
        extractJsonString(jsonBody, "username", usernameText);
        extractJsonString(jsonBody, "password", passwordText);
        extractJsonInt(jsonBody, "port", portValue);
    }
    if (portValue < 1 || portValue > 65535) {
        portValue = 80;
    }
    if (hostText.length() == 0 && urlText.length() > 0) {
        char parsedHost[CAMERA_HOST_MAX];
        uint16_t parsedPort = 80;
        if (parseHttpUrl(urlText.c_str(), parsedHost, sizeof(parsedHost), &parsedPort)) {
            hostText = parsedHost;
            portValue = parsedPort;
        }
    }

    // Display/snapshot URLs (cgi-bin, jpg, rtsp) are NOT ONVIF device services.
    // Prefer host + common device_service paths; only use url when it looks like ONVIF.
    const bool urlLooksOnvif = urlText.length() > 0
        && (urlText.startsWith("http://") || urlText.startsWith("https://"))
        && (urlText.indexOf("/onvif/") >= 0
            || urlText.indexOf("device_service") >= 0
            || urlText.indexOf("media_service") >= 0);

    static const char *kDevicePaths[] = {
        "/onvif/device_service",
        "/onvif/device",
        "/onvif/services",
        "/onvif/Devices",
        "/device_service",
    };
    constexpr int kCandidateMax = 8;
    char (*candidates)[CAMERA_URL_MAX] = static_cast<char (*)[CAMERA_URL_MAX]>(
        allocatePsram(sizeof(char) * kCandidateMax * CAMERA_URL_MAX)
    );
    OnvifResolvedMedia *resolved = static_cast<OnvifResolvedMedia *>(
        allocatePsram(sizeof(OnvifResolvedMedia))
    );
    if (candidates == nullptr || resolved == nullptr) {
        if (candidates != nullptr) {
            free(candidates);
        }
        if (resolved != nullptr) {
            free(resolved);
        }
        return String("{\"ok\":false,\"error\":\"out of memory\"}");
    }
    memset(candidates, 0, sizeof(char) * kCandidateMax * CAMERA_URL_MAX);
    memset(resolved, 0, sizeof(*resolved));
    int candidateCount = 0;
    auto addCandidate = [&](const char *candidate) {
        if (candidate == nullptr || candidate[0] == '\0' || candidateCount >= kCandidateMax) {
            return;
        }
        for (int index = 0; index < candidateCount; index++) {
            if (strcasecmp(candidates[index], candidate) == 0) {
                return;
            }
        }
        strncpy(candidates[candidateCount], candidate, CAMERA_URL_MAX - 1);
        candidates[candidateCount][CAMERA_URL_MAX - 1] = '\0';
        candidateCount++;
    };

    // Prefer the auto-filled / Discover XAddr first. Still add common host paths so a
    // default device_service guess can recover without the operator typing a path.
    // Successful first candidate exits immediately (Discover path stays fast).
    if (urlLooksOnvif) {
        addCandidate(urlText.c_str());
        LOGGER.info(String("Camera resolve: prefer device URL ") + urlText);
    }
    if (hostText.length() > 0) {
        for (size_t pathIndex = 0; pathIndex < sizeof(kDevicePaths) / sizeof(kDevicePaths[0]);
             pathIndex++) {
            char built[CAMERA_URL_MAX];
            snprintf(
                built,
                sizeof(built),
                "http://%s:%d%s",
                hostText.c_str(),
                portValue,
                kDevicePaths[pathIndex]
            );
            addCandidate(built);
        }
        // Some cameras expose ONVIF on 8000/8080/8899 while HTTP snapshot stays on 80.
        if (portValue == 80 && !urlLooksOnvif) {
            const int altPorts[] = {8000, 8080, 8899, 5000};
            for (size_t altIndex = 0; altIndex < sizeof(altPorts) / sizeof(altPorts[0]); altIndex++) {
                char built[CAMERA_URL_MAX];
                snprintf(
                    built,
                    sizeof(built),
                    "http://%s:%d/onvif/device_service",
                    hostText.c_str(),
                    altPorts[altIndex]
                );
                addCandidate(built);
            }
        }
    }

    LOGGER.info(
        String("Camera resolve: start host=")
        + (hostText.length() > 0 ? hostText : "(none)")
        + " port=" + String(portValue)
        + " user=" + (usernameText.length() > 0 ? "yes" : "no")
        + " candidates=" + String(candidateCount)
    );

    PsramJsonBuffer json;
    char errorText[CAMERA_STATUS_ERROR_MAX];
    errorText[0] = '\0';
    bool ok = false;
    if (candidateCount == 0) {
        strncpy(errorText, "missing ONVIF device URL/host", sizeof(errorText) - 1);
        LOGGER.warning("Camera resolve: missing ONVIF device URL/host");
    } else {
        for (int candidateIndex = 0; candidateIndex < candidateCount; candidateIndex++) {
            char attemptError[CAMERA_STATUS_ERROR_MAX];
            attemptError[0] = '\0';
            LOGGER.info(
                String("Camera resolve: try ") + candidates[candidateIndex]
            );
            // Resolve directly into `resolved` to avoid a second large stack/PSRAM clone.
            OnvifResolvedMedia *attempt = resolved;
            memset(attempt, 0, sizeof(*attempt));
            if (onvifResolveMedia(
                    candidates[candidateIndex],
                    usernameText.c_str(),
                    passwordText.c_str(),
                    attempt,
                    attemptError,
                    sizeof(attemptError)
                )) {
                ok = true;
                errorText[0] = '\0';
                break;
            }
            strncpy(errorText, attemptError, sizeof(errorText) - 1);
            errorText[sizeof(errorText) - 1] = '\0';
            LOGGER.warning(
                String("Camera resolve: fail ") + candidates[candidateIndex]
                + " -> " + errorText
            );
            yield();
        }
    }
    if (ok) {
        // Fill the dialog with ONVIF URIs as returned — do not HTTP-probe snapshots here.
        // Operator uses Test explicitly when they want a live fetch check.
        LOGGER.info(
            String("Camera resolve: OK name=") + resolved->name
            + " device=" + resolved->url
        );
        for (int channelIndex = 0; channelIndex < CAMERA_CHANNELS; channelIndex++) {
            LOGGER.info(
                String("Camera resolve: Ch") + String(channelIndex + 1)
                + " snapshot="
                + (resolved->snapshotUrl[channelIndex][0] != '\0'
                       ? resolved->snapshotUrl[channelIndex]
                       : "(empty)")
                + " live="
                + (resolved->liveUrl[channelIndex][0] != '\0'
                       ? resolved->liveUrl[channelIndex]
                       : "(empty)")
            );
        }
        if (resolved->hint[0] != '\0') {
            LOGGER.info(String("Camera resolve: hint=") + resolved->hint);
        }
    } else {
        LOGGER.warning(
            String("Camera resolve: failed host=")
            + (hostText.length() > 0 ? hostText : "(none)")
            + " error=" + errorText
        );
    }
    json += "{\"ok\":";
    json += ok ? "true" : "false";
    json += ",\"error\":\"";
    appendJsonEscaped(json, errorText, CAMERA_STATUS_ERROR_MAX);
    json += "\",\"hint\":\"";
    appendJsonEscaped(json, resolved->hint, CAMERA_SNAPSHOT_HINT_MAX);
    json += "\",\"name\":\"";
    appendJsonEscaped(json, resolved->name, CAMERA_NAME_MAX);
    json += "\",\"url\":\"";
    appendJsonEscaped(json, resolved->url, CAMERA_URL_MAX);
    json += "\",\"host\":\"";
    appendJsonEscaped(json, resolved->host, CAMERA_HOST_MAX);
    json += "\",\"port\":";
    json += String((unsigned)(resolved->port != 0 ? resolved->port : (uint16_t)portValue));
    json += ",\"channels\":[";
    for (int channelIndex = 0; channelIndex < CAMERA_CHANNELS; channelIndex++) {
        if (channelIndex > 0) {
            json += ",";
        }
        json += "{\"snapshotUrl\":\"";
        appendJsonEscaped(json, resolved->snapshotUrl[channelIndex], CAMERA_URL_MAX);
        json += "\",\"liveUrl\":\"";
        appendJsonEscaped(json, resolved->liveUrl[channelIndex], CAMERA_URL_MAX);
        json += "\"}";
    }
    json += "]}";
    free(candidates);
    free(resolved);
    if (!json.ok()) {
        return String("{\"ok\":false,\"error\":\"out of memory\"}");
    }
    return json.toString();
}

String CameraStore::testSnapshotsJson(const char *jsonBody) const {
    String usernameText;
    String passwordText;
    String snapshotUrl[CAMERA_CHANNELS];
    if (jsonBody != nullptr) {
        extractJsonString(jsonBody, "username", usernameText);
        extractJsonString(jsonBody, "password", passwordText);
        extractJsonString(jsonBody, "snapshotUrl1", snapshotUrl[0]);
        extractJsonString(jsonBody, "snapshotUrl2", snapshotUrl[1]);
        if (snapshotUrl[0].length() == 0) {
            extractJsonString(jsonBody, "snapshotUrl", snapshotUrl[0]);
        }
    }

    LOGGER.info(
        String("Camera test: start user=")
        + (usernameText.length() > 0 ? "yes" : "no")
        + " Ch1=" + (snapshotUrl[0].length() > 0 ? snapshotUrl[0] : "(empty)")
        + " Ch2=" + (snapshotUrl[1].length() > 0 ? snapshotUrl[1] : "(empty)")
    );

    constexpr size_t kProbeMax = 64UL * 1024UL;
    PsramJsonBuffer json;
    json += "{\"channels\":[";
    bool allOk = true;
    bool anyUrl = false;
    char summaryError[CAMERA_STATUS_ERROR_MAX];
    summaryError[0] = '\0';

    for (int channelIndex = 0; channelIndex < CAMERA_CHANNELS; channelIndex++) {
        if (channelIndex > 0) {
            json += ",";
            delay(CAMERA_SNAPSHOT_CHANNEL_GAP_MS);
            yield();
        }
        const String &url = snapshotUrl[channelIndex];
        json += "{\"channel\":";
        json += String(channelIndex + 1);
        json += ",\"snapshotUrl\":\"";
        appendJsonEscaped(json, url.c_str(), CAMERA_URL_MAX);
        json += "\"";

        if (url.length() == 0) {
            // Unconfigured channel: skip (not a failure).
            json += ",\"ok\":true,\"bytes\":0,\"error\":\"\",\"skipped\":true}";
            LOGGER.info(
                String("Camera test: Ch") + String(channelIndex + 1) + " skipped (empty URL)"
            );
            continue;
        }
        anyUrl = true;

        bool channelOk = false;
        size_t bytes = 0;
        char channelError[CAMERA_STATUS_ERROR_MAX];
        channelError[0] = '\0';
        uint8_t *buffer = static_cast<uint8_t *>(allocatePsram(kProbeMax));
        if (buffer == nullptr) {
            strncpy(channelError, "out of PSRAM", sizeof(channelError) - 1);
        } else {
            channelOk = httpFetchSnapshotBody(
                url.c_str(),
                usernameText.c_str(),
                passwordText.c_str(),
                buffer,
                kProbeMax,
                &bytes,
                channelError,
                sizeof(channelError)
            );
            free(buffer);
        }

        if (!channelOk) {
            allOk = false;
            if (summaryError[0] == '\0') {
                snprintf(
                    summaryError,
                    sizeof(summaryError),
                    "channel %d: %s",
                    channelIndex + 1,
                    channelError[0] != '\0' ? channelError : "failed"
                );
            }
            LOGGER.warning(
                String("Camera test: Ch") + String(channelIndex + 1) + " FAIL "
                + (channelError[0] != '\0' ? channelError : "failed")
                + " url=" + url
            );
        } else {
            LOGGER.info(
                String("Camera test: Ch") + String(channelIndex + 1) + " OK bytes="
                + String((unsigned)bytes)
            );
        }
        json += ",\"ok\":";
        json += channelOk ? "true" : "false";
        json += ",\"bytes\":";
        json += String((unsigned)bytes);
        json += ",\"error\":\"";
        appendJsonEscaped(json, channelError, CAMERA_STATUS_ERROR_MAX);
        json += "\"}";
        yield();
    }

    json += "],\"ok\":";
    if (!anyUrl) {
        allOk = false;
        if (summaryError[0] == '\0') {
            strncpy(summaryError, "no snapshot URLs", sizeof(summaryError) - 1);
        }
    }
    json += allOk ? "true" : "false";
    json += ",\"error\":\"";
    appendJsonEscaped(json, summaryError, CAMERA_STATUS_ERROR_MAX);
    json += "\"}";
    LOGGER.info(
        String("Camera test: done ok=") + (allOk ? "true" : "false")
        + (summaryError[0] != '\0' ? String(" error=") + summaryError : String(""))
    );
    if (!json.ok()) {
        return String("{\"ok\":false,\"error\":\"out of memory\",\"channels\":[]}");
    }
    return json.toString();
}

uint8_t *CameraStore::fetchSnapshotJpeg(const char *cameraId, uint8_t channel, size_t *outSize) {
    if (outSize != nullptr) {
        *outSize = 0;
    }
    if (cameraId == nullptr || cameraId[0] == '\0' || outSize == nullptr) {
        return nullptr;
    }
    const CameraRecord *camera = findById(cameraId);
    if (camera == nullptr) {
        return nullptr;
    }
    const uint8_t channelIndex = (channel == 2) ? 1 : 0;
    const char *snapshotUrl = camera->onvif.channels[channelIndex].snapshotUrl;
    if (snapshotUrl == nullptr || snapshotUrl[0] == '\0') {
        return nullptr;
    }

    uint8_t *buffer = static_cast<uint8_t *>(allocatePsram(CAMERA_EMAIL_JPEG_MAX));
    if (buffer == nullptr) {
        LOGGER.warning("Camera snapshot ignored: out of PSRAM");
        return nullptr;
    }

    size_t bytes = 0;
    char errorText[CAMERA_STATUS_ERROR_MAX];
    errorText[0] = '\0';
    const bool ok = httpFetchSnapshotBody(
        snapshotUrl,
        camera->onvif.username,
        camera->onvif.password,
        buffer,
        CAMERA_EMAIL_JPEG_MAX,
        &bytes,
        errorText,
        sizeof(errorText)
    );
    if (!ok || bytes == 0) {
        LOGGER.warning(
            String("Camera snapshot fetch failed: ")
            + (errorText[0] != '\0' ? errorText : "unknown")
        );
        free(buffer);
        return nullptr;
    }
    *outSize = bytes;
    return buffer;
}
