#include "OnvifClient.h"
#include "Logger.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <esp_heap_caps.h>
#include <esp_random.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha1.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

namespace {

constexpr size_t kOnvifSoapMax = 48 * 1024;
constexpr size_t kOnvifSoapMin = 32;

void *allocatePsram(size_t bytes) {
    if (bytes == 0) {
        return nullptr;
    }
    const size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const size_t maxBlock = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    if (freePsram < bytes || maxBlock < bytes) {
        return nullptr;
    }
    void *block = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (block == nullptr) {
        block = ps_malloc(bytes);
    }
    return block;
}

void setError(char *errorText, size_t errorMax, const char *message) {
    if (errorText == nullptr || errorMax == 0) {
        return;
    }
    if (message == nullptr) {
        errorText[0] = '\0';
        return;
    }
    strncpy(errorText, message, errorMax - 1);
    errorText[errorMax - 1] = '\0';
}

bool parseHttpUrl(const char *url, char *hostOut, size_t hostMax, uint16_t *portOut) {
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

void copyBounded(char *destination, size_t destinationMax, const char *source, size_t sourceLength) {
    if (destination == nullptr || destinationMax == 0) {
        return;
    }
    if (source == nullptr) {
        destination[0] = '\0';
        return;
    }
    if (sourceLength >= destinationMax) {
        sourceLength = destinationMax - 1;
    }
    memcpy(destination, source, sourceLength);
    destination[sourceLength] = '\0';
}

void unescapeXmlInPlace(char *text) {
    if (text == nullptr || text[0] == '\0') {
        return;
    }
    char *read = text;
    char *write = text;
    while (*read != '\0') {
        if (read[0] == '&') {
            if (strncmp(read, "&amp;", 5) == 0) {
                *write++ = '&';
                read += 5;
                continue;
            }
            if (strncmp(read, "&lt;", 4) == 0) {
                *write++ = '<';
                read += 4;
                continue;
            }
            if (strncmp(read, "&gt;", 4) == 0) {
                *write++ = '>';
                read += 4;
                continue;
            }
            if (strncmp(read, "&quot;", 6) == 0) {
                *write++ = '"';
                read += 6;
                continue;
            }
            if (strncmp(read, "&apos;", 6) == 0) {
                *write++ = '\'';
                read += 6;
                continue;
            }
        }
        *write++ = *read++;
    }
    *write = '\0';
}

bool extractTagText(
    const char *xml,
    const char *localName,
    char *out,
    size_t outMax,
    const char **resumeFrom = nullptr
) {
    if (xml == nullptr || localName == nullptr || out == nullptr || outMax == 0) {
        return false;
    }
    out[0] = '\0';
    const char *searchFrom = resumeFrom != nullptr && *resumeFrom != nullptr ? *resumeFrom : xml;
    const size_t nameLength = strlen(localName);
    const char *cursor = searchFrom;
    while ((cursor = strstr(cursor, localName)) != nullptr) {
        if (cursor > searchFrom) {
            const char before = cursor[-1];
            if (before != ':' && before != '<' && before != '/') {
                cursor += nameLength;
                continue;
            }
        }
        const char after = cursor[nameLength];
        if (after != '>' && after != ' ' && after != '\t' && after != '\r' && after != '\n'
            && after != '/') {
            cursor += nameLength;
            continue;
        }
        const char *gt = strchr(cursor, '>');
        if (gt == nullptr) {
            return false;
        }
        if (gt > cursor && gt[-1] == '/') {
            // Empty element <XAddr/>.
            cursor = gt + 1;
            continue;
        }
        const char *content = gt + 1;
        const char *end = strstr(content, "</");
        if (end == nullptr || end <= content) {
            return false;
        }
        while (content < end && (*content == ' ' || *content == '\n' || *content == '\r' || *content == '\t')) {
            content++;
        }
        const char *trimEnd = end;
        while (trimEnd > content
            && (trimEnd[-1] == ' ' || trimEnd[-1] == '\n' || trimEnd[-1] == '\r' || trimEnd[-1] == '\t')) {
            trimEnd--;
        }
        copyBounded(out, outMax, content, (size_t)(trimEnd - content));
        unescapeXmlInPlace(out);
        if (resumeFrom != nullptr) {
            *resumeFrom = end;
        }
        return out[0] != '\0';
    }
    return false;
}

bool extractAttribute(
    const char *xml,
    const char *elementHint,
    const char *attrName,
    char *out,
    size_t outMax,
    const char **resumeFrom = nullptr
) {
    if (xml == nullptr || elementHint == nullptr || attrName == nullptr || out == nullptr || outMax == 0) {
        return false;
    }
    out[0] = '\0';
    const char *searchFrom = resumeFrom != nullptr && *resumeFrom != nullptr ? *resumeFrom : xml;
    const size_t hintLength = strlen(elementHint);
    const char *cursor = searchFrom;
    while ((cursor = strstr(cursor, elementHint)) != nullptr) {
        if (cursor > searchFrom) {
            const char before = cursor[-1];
            if (before != ':' && before != '<') {
                cursor += hintLength;
                continue;
            }
        }
        const char after = cursor[hintLength];
        if (after != '>' && after != ' ' && after != '\t' && after != '\r' && after != '\n') {
            cursor += hintLength;
            continue;
        }
        const char *gt = strchr(cursor, '>');
        if (gt == nullptr) {
            return false;
        }
        char attrPattern[48];
        snprintf(attrPattern, sizeof(attrPattern), "%s=\"", attrName);
        const char *attr = strstr(cursor, attrPattern);
        if (attr == nullptr || attr > gt) {
            snprintf(attrPattern, sizeof(attrPattern), "%s='", attrName);
            attr = strstr(cursor, attrPattern);
            if (attr == nullptr || attr > gt) {
                cursor = gt + 1;
                continue;
            }
        }
        attr += strlen(attrPattern);
        const char quote = attrPattern[strlen(attrPattern) - 1];
        const char *end = strchr(attr, quote);
        if (end == nullptr || end <= attr) {
            cursor = gt + 1;
            continue;
        }
        copyBounded(out, outMax, attr, (size_t)(end - attr));
        unescapeXmlInPlace(out);
        if (resumeFrom != nullptr) {
            *resumeFrom = gt + 1;
        }
        return out[0] != '\0';
    }
    return false;
}

bool looksLikeUri(const char *text) {
    return text != nullptr
        && (strncmp(text, "http://", 7) == 0 || strncmp(text, "https://", 8) == 0
            || strncmp(text, "rtsp://", 7) == 0);
}

bool xmlHasFault(const char *xml) {
    return xml != nullptr && (strstr(xml, "Fault>") != nullptr || strstr(xml, ":Fault") != nullptr);
}

bool xmlLooksUnauthorized(const char *xml) {
    if (xml == nullptr) {
        return false;
    }
    return strstr(xml, "NotAuthorized") != nullptr
        || strstr(xml, "not authorized") != nullptr
        || strstr(xml, "Sender not Authorized") != nullptr
        || strstr(xml, "Unauthorized") != nullptr;
}

bool encodeBase64(const unsigned char *input, size_t inputLength, char *out, size_t outMax) {
    if (input == nullptr || out == nullptr || outMax == 0) {
        return false;
    }
    size_t written = 0;
    if (mbedtls_base64_encode(
            reinterpret_cast<unsigned char *>(out),
            outMax - 1,
            &written,
            input,
            inputLength
        )
        != 0) {
        out[0] = '\0';
        return false;
    }
    out[written] = '\0';
    return true;
}

void fillCreatedUtc(char *out, size_t outMax) {
    if (out == nullptr || outMax < 25) {
        return;
    }
    time_t now = time(nullptr);
    // Without NTP many boards report epoch; ONVIF digest still needs a plausible stamp.
    if (now < 1700000000L) {
        strncpy(out, "2024-06-15T12:00:00.000Z", outMax - 1);
        out[outMax - 1] = '\0';
        return;
    }
    struct tm utc = {};
    gmtime_r(&now, &utc);
    snprintf(
        out,
        outMax,
        "%04d-%02d-%02dT%02d:%02d:%02d.000Z",
        utc.tm_year + 1900,
        utc.tm_mon + 1,
        utc.tm_mday,
        utc.tm_hour,
        utc.tm_min,
        utc.tm_sec
    );
}

enum OnvifAuthMode {
    OnvifAuthNone = 0,
    OnvifAuthPasswordText,
    OnvifAuthPasswordDigest
};

void appendSecurityHeader(
    String &soap,
    const char *username,
    const char *password,
    OnvifAuthMode authMode
) {
    if (username == nullptr || username[0] == '\0' || authMode == OnvifAuthNone) {
        return;
    }
    soap +=
        "<s:Header><wsse:Security xmlns:wsse=\"http://docs.oasis-open.org/wss/2004/01/"
        "oasis-200401-wss-wssecurity-secext-1.0.xsd\" xmlns:wsu=\"http://docs.oasis-open.org/wss/2004/01/"
        "oasis-200401-wss-wssecurity-utility-1.0.xsd\">"
        "<wsse:UsernameToken>"
        "<wsse:Username>";
    soap += username;
    soap += "</wsse:Username>";
    if (authMode == OnvifAuthPasswordDigest) {
        unsigned char nonceRaw[16];
        esp_fill_random(nonceRaw, sizeof(nonceRaw));
        char nonceB64[32];
        if (!encodeBase64(nonceRaw, sizeof(nonceRaw), nonceB64, sizeof(nonceB64))) {
            soap +=
                "<wsse:Password Type=\"http://docs.oasis-open.org/wss/2004/01/"
                "oasis-200401-wss-username-token-profile-1.0#PasswordText\">";
            soap += password != nullptr ? password : "";
            soap += "</wsse:Password>";
        } else {
            char created[40];
            fillCreatedUtc(created, sizeof(created));
            const char *passwordText = password != nullptr ? password : "";
            const size_t createdLength = strlen(created);
            const size_t passwordLength = strlen(passwordText);
            const size_t digestInputLength = sizeof(nonceRaw) + createdLength + passwordLength;
            unsigned char *digestInput = static_cast<unsigned char *>(allocatePsram(digestInputLength));
            unsigned char digestRaw[20];
            if (digestInput == nullptr) {
                soap +=
                    "<wsse:Password Type=\"http://docs.oasis-open.org/wss/2004/01/"
                    "oasis-200401-wss-username-token-profile-1.0#PasswordText\">";
                soap += passwordText;
                soap += "</wsse:Password></wsse:UsernameToken></wsse:Security></s:Header>";
                return;
            }
            memcpy(digestInput, nonceRaw, sizeof(nonceRaw));
            memcpy(digestInput + sizeof(nonceRaw), created, createdLength);
            memcpy(digestInput + sizeof(nonceRaw) + createdLength, passwordText, passwordLength);
            mbedtls_sha1(digestInput, digestInputLength, digestRaw);
            free(digestInput);
            char digestB64[40];
            if (!encodeBase64(digestRaw, sizeof(digestRaw), digestB64, sizeof(digestB64))) {
                digestB64[0] = '\0';
            }
            soap += "<wsse:Password Type=\"http://docs.oasis-open.org/wss/2004/01/"
                    "oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">";
            soap += digestB64;
            soap += "</wsse:Password><wsse:Nonce EncodingType=\"http://docs.oasis-open.org/wss/2004/01/"
                    "oasis-200401-wss-soap-message-security-1.0#Base64Binary\">";
            soap += nonceB64;
            soap += "</wsse:Nonce><wsu:Created>";
            soap += created;
            soap += "</wsu:Created>";
        }
    } else {
        soap +=
            "<wsse:Password Type=\"http://docs.oasis-open.org/wss/2004/01/"
            "oasis-200401-wss-username-token-profile-1.0#PasswordText\">";
        soap += password != nullptr ? password : "";
        soap += "</wsse:Password>";
    }
    soap += "</wsse:UsernameToken></wsse:Security></s:Header>";
}

char *soapPostOnce(
    const char *url,
    const char *username,
    const char *password,
    const char *soapAction,
    const String &body,
    int *httpStatusOut
) {
    if (httpStatusOut != nullptr) {
        *httpStatusOut = 0;
    }
    if (url == nullptr || url[0] == '\0') {
        return nullptr;
    }
    HTTPClient http;
    http.setReuse(false);
    http.setTimeout(10000);
    if (!http.begin(url)) {
        return nullptr;
    }
    if (username != nullptr && username[0] != '\0') {
        http.setAuthorization(username, password != nullptr ? password : "");
    }
    // SOAP 1.1 — widely accepted by ONVIF cameras (SOAP 1.2 alone often fails).
    http.addHeader("Content-Type", "text/xml; charset=utf-8");
    http.addHeader("Accept", "application/soap+xml, application/xml, text/xml, */*");
    if (soapAction != nullptr && soapAction[0] != '\0') {
        String actionHeader = "\"";
        actionHeader += soapAction;
        actionHeader += "\"";
        http.addHeader("SOAPAction", actionHeader);
    }
    const int status = http.POST(body);
    if (httpStatusOut != nullptr) {
        *httpStatusOut = status;
    }
    if (status <= 0) {
        http.end();
        return nullptr;
    }

    char *copy = static_cast<char *>(allocatePsram(kOnvifSoapMax));
    if (copy == nullptr) {
        http.end();
        return nullptr;
    }

    size_t total = 0;
    const int contentLength = http.getSize();
    WiFiClient *stream = http.getStreamPtr();
    if (stream != nullptr) {
        const uint32_t deadlineMs = millis() + 10000;
        while (total + 1 < kOnvifSoapMax && millis() < deadlineMs) {
            if (contentLength > 0 && (int)total >= contentLength) {
                break;
            }
            const size_t available = stream->available();
            if (available == 0) {
                if (!http.connected() && (contentLength <= 0 || (int)total >= contentLength)) {
                    break;
                }
                if (!http.connected() && total >= kOnvifSoapMin) {
                    break;
                }
                delay(2);
                yield();
                continue;
            }
            size_t toRead = available;
            if (toRead > kOnvifSoapMax - 1 - total) {
                toRead = kOnvifSoapMax - 1 - total;
            }
            const int readCount = stream->readBytes(copy + total, toRead);
            if (readCount <= 0) {
                break;
            }
            total += (size_t)readCount;
            yield();
        }
    }
    if (total == 0) {
        const String response = http.getString();
        http.end();
        if (response.length() == 0 || response.length() + 1 > kOnvifSoapMax) {
            free(copy);
            return nullptr;
        }
        memcpy(copy, response.c_str(), response.length());
        copy[response.length()] = '\0';
        return copy;
    }
    http.end();
    copy[total] = '\0';
    if (total < kOnvifSoapMin) {
        free(copy);
        return nullptr;
    }
    return copy;
}

String buildSoapEnvelope(
    const char *bodyInnerXml,
    const char *username,
    const char *password,
    OnvifAuthMode authMode,
    const char *extraXmlns
) {
    String soap = "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                  "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"";
    if (extraXmlns != nullptr && extraXmlns[0] != '\0') {
        soap += " ";
        soap += extraXmlns;
    }
    soap += ">";
    appendSecurityHeader(soap, username, password, authMode);
    soap += "<s:Body>";
    soap += bodyInnerXml;
    soap += "</s:Body></s:Envelope>";
    return soap;
}

char *soapCall(
    const char *url,
    const char *username,
    const char *password,
    OnvifAuthMode authMode,
    const char *soapAction,
    const char *bodyInnerXml,
    const char *extraXmlns,
    int *httpStatusOut
) {
    const String envelope = buildSoapEnvelope(bodyInnerXml, username, password, authMode, extraXmlns);
    return soapPostOnce(url, username, password, soapAction, envelope, httpStatusOut);
}

bool extractMediaXaddr(const char *capabilitiesXml, const char *sectionHint, char *out, size_t outMax) {
    if (out == nullptr || outMax == 0) {
        return false;
    }
    out[0] = '\0';
    if (capabilitiesXml == nullptr || sectionHint == nullptr) {
        return false;
    }
    const char *section = strstr(capabilitiesXml, sectionHint);
    if (section == nullptr) {
        return false;
    }
    // Prefer an element boundary (:Media> / <tt:Media ...) rather than the word inside a namespace URI.
    const char *bounded = section;
    bool foundBoundary = false;
    for (const char *cursor = capabilitiesXml; (cursor = strstr(cursor, sectionHint)) != nullptr;
         cursor += strlen(sectionHint)) {
        if (cursor > capabilitiesXml) {
            const char before = cursor[-1];
            if (before != ':' && before != '<') {
                continue;
            }
        }
        const char after = cursor[strlen(sectionHint)];
        if (after == '>' || after == ' ' || after == '\t' || after == '\r' || after == '\n') {
            bounded = cursor;
            foundBoundary = true;
            break;
        }
    }
    if (!foundBoundary) {
        return false;
    }
    const char *resume = bounded;
    return extractTagText(resume, "XAddr", out, outMax, &resume) && looksLikeUri(out);
}

int collectProfileTokens(const char *profilesXml, char tokens[][64], int maxTokens) {
    if (profilesXml == nullptr || tokens == nullptr || maxTokens <= 0) {
        return 0;
    }
    int count = 0;
    const char *resume = profilesXml;
    while (count < maxTokens) {
        char token[64];
        if (!extractAttribute(resume, "Profiles", "token", token, sizeof(token), &resume)) {
            if (!extractAttribute(resume, "Profile", "token", token, sizeof(token), &resume)) {
                break;
            }
        }
        // Skip duplicates.
        bool duplicate = false;
        for (int index = 0; index < count; index++) {
            if (strcmp(tokens[index], token) == 0) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }
        strncpy(tokens[count], token, 63);
        tokens[count][63] = '\0';
        count++;
    }
    return count;
}

bool fillUrisForProfiles(
    const char *mediaXaddr,
    const char *username,
    const char *password,
    OnvifAuthMode authMode,
    const char *mediaXmlns,
    const char *snapshotAction,
    const char *streamAction,
    const char profileTokens[][64],
    int profileCount,
    OnvifResolvedMedia *out
) {
    if (mediaXaddr == nullptr || out == nullptr || profileCount <= 0) {
        return false;
    }
    bool anyUrl = false;
    for (int channelIndex = 0; channelIndex < profileCount && channelIndex < CAMERA_CHANNELS;
         channelIndex++) {
        char snapshotBody[256];
        snprintf(
            snapshotBody,
            sizeof(snapshotBody),
            "<trt:GetSnapshotUri xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">"
            "<trt:ProfileToken>%s</trt:ProfileToken></trt:GetSnapshotUri>",
            profileTokens[channelIndex]
        );
        // Media2 uses tr2 prefix in some firmwares; keep trt namespaced by xmlns on the element.
        if (mediaXmlns != nullptr && strstr(mediaXmlns, "ver20/media") != nullptr) {
            snprintf(
                snapshotBody,
                sizeof(snapshotBody),
                "<tr2:GetSnapshotUri xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">"
                "<tr2:ProfileToken>%s</tr2:ProfileToken></tr2:GetSnapshotUri>",
                profileTokens[channelIndex]
            );
        }
        int status = 0;
        char *snapshotXml = soapCall(
            mediaXaddr,
            username,
            password,
            authMode,
            snapshotAction,
            snapshotBody,
            nullptr,
            &status
        );
        if (snapshotXml != nullptr && !xmlHasFault(snapshotXml)) {
            char uri[CAMERA_URL_MAX];
            uri[0] = '\0';
            if (extractTagText(snapshotXml, "Uri", uri, sizeof(uri)) && looksLikeUri(uri)) {
                strncpy(out->snapshotUrl[channelIndex], uri, CAMERA_URL_MAX - 1);
                anyUrl = true;
            }
        }
        if (snapshotXml != nullptr) {
            free(snapshotXml);
        }

        char streamBody[512];
        if (mediaXmlns != nullptr && strstr(mediaXmlns, "ver20/media") != nullptr) {
            snprintf(
                streamBody,
                sizeof(streamBody),
                "<tr2:GetStreamUri xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">"
                "<tr2:Protocol>RTSP</tr2:Protocol>"
                "<tr2:ProfileToken>%s</tr2:ProfileToken></tr2:GetStreamUri>",
                profileTokens[channelIndex]
            );
        } else {
            snprintf(
                streamBody,
                sizeof(streamBody),
                "<trt:GetStreamUri xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
                "xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
                "<trt:StreamSetup><tt:Stream>RTP-Unicast</tt:Stream>"
                "<tt:Transport><tt:Protocol>RTSP</tt:Protocol></tt:Transport></trt:StreamSetup>"
                "<trt:ProfileToken>%s</trt:ProfileToken></trt:GetStreamUri>",
                profileTokens[channelIndex]
            );
        }
        char *streamXml = soapCall(
            mediaXaddr,
            username,
            password,
            authMode,
            streamAction,
            streamBody,
            nullptr,
            &status
        );
        if (streamXml != nullptr && !xmlHasFault(streamXml)) {
            char uri[CAMERA_URL_MAX];
            uri[0] = '\0';
            if (extractTagText(streamXml, "Uri", uri, sizeof(uri)) && looksLikeUri(uri)) {
                strncpy(out->liveUrl[channelIndex], uri, CAMERA_URL_MAX - 1);
                anyUrl = true;
            }
        }
        if (streamXml != nullptr) {
            free(streamXml);
        }
        yield();
    }
    return anyUrl;
}

bool resolveWithAuth(
    const char *deviceServiceUrl,
    const char *username,
    const char *password,
    OnvifAuthMode authMode,
    OnvifResolvedMedia *out,
    char *errorText,
    size_t errorMax
) {
    int status = 0;
    char *capabilitiesXml = soapCall(
        deviceServiceUrl,
        username,
        password,
        authMode,
        "http://www.onvif.org/ver10/device/wsdl/GetCapabilities",
        "<tds:GetCapabilities xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">"
        "<tds:Category>All</tds:Category></tds:GetCapabilities>",
        "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\"",
        &status
    );
    if (capabilitiesXml == nullptr) {
        setError(errorText, errorMax, "GetCapabilities failed");
        return false;
    }
    if (xmlHasFault(capabilitiesXml)) {
        const bool unauthorized = xmlLooksUnauthorized(capabilitiesXml) || status == 401 || status == 403;
        free(capabilitiesXml);
        setError(errorText, errorMax, unauthorized ? "ONVIF unauthorized" : "GetCapabilities fault");
        return false;
    }

    char mediaXaddr[CAMERA_URL_MAX];
    char media2Xaddr[CAMERA_URL_MAX];
    mediaXaddr[0] = '\0';
    media2Xaddr[0] = '\0';
    extractMediaXaddr(capabilitiesXml, "Media2", media2Xaddr, sizeof(media2Xaddr));
    extractMediaXaddr(capabilitiesXml, "Media", mediaXaddr, sizeof(mediaXaddr));
    // If "Media" matched Media2 first, Media may be empty — try plain first XAddr under Device as last resort.
    if (mediaXaddr[0] == '\0' || !looksLikeUri(mediaXaddr)) {
        // Avoid grabbing Media2 again: search for token that is exactly Media not Media2.
        const char *cursor = capabilitiesXml;
        while ((cursor = strstr(cursor, "Media")) != nullptr) {
            if (strncmp(cursor, "Media2", 6) == 0) {
                cursor += 6;
                continue;
            }
            if (cursor > capabilitiesXml) {
                const char before = cursor[-1];
                if (before != ':' && before != '<') {
                    cursor += 5;
                    continue;
                }
            }
            const char after = cursor[5];
            if (after != '>' && after != ' ' && after != '\t' && after != '\r' && after != '\n') {
                cursor += 5;
                continue;
            }
            const char *resume = cursor;
            if (extractTagText(resume, "XAddr", mediaXaddr, sizeof(mediaXaddr), &resume)
                && looksLikeUri(mediaXaddr)) {
                break;
            }
            cursor += 5;
        }
    }
    free(capabilitiesXml);

    if ((mediaXaddr[0] == '\0' || !looksLikeUri(mediaXaddr))
        && (media2Xaddr[0] == '\0' || !looksLikeUri(media2Xaddr))) {
        strncpy(mediaXaddr, deviceServiceUrl, sizeof(mediaXaddr) - 1);
    }

    {
        char *infoXml = soapCall(
            deviceServiceUrl,
            username,
            password,
            authMode,
            "http://www.onvif.org/ver10/device/wsdl/GetDeviceInformation",
            "<tds:GetDeviceInformation xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\"/>",
            "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\"",
            &status
        );
        if (infoXml != nullptr && !xmlHasFault(infoXml)) {
            char model[CAMERA_NAME_MAX];
            char manufacturer[CAMERA_NAME_MAX];
            model[0] = '\0';
            manufacturer[0] = '\0';
            extractTagText(infoXml, "Model", model, sizeof(model));
            extractTagText(infoXml, "Manufacturer", manufacturer, sizeof(manufacturer));
            if (manufacturer[0] != '\0' && model[0] != '\0') {
                snprintf(out->name, sizeof(out->name), "%s %s", manufacturer, model);
            } else if (model[0] != '\0') {
                strncpy(out->name, model, sizeof(out->name) - 1);
            } else if (manufacturer[0] != '\0') {
                strncpy(out->name, manufacturer, sizeof(out->name) - 1);
            }
        }
        if (infoXml != nullptr) {
            free(infoXml);
        }
    }

    char profileTokens[CAMERA_CHANNELS][64];
    memset(profileTokens, 0, sizeof(profileTokens));
    int profileCount = 0;
    const char *usedMediaXaddr = nullptr;
    const char *usedMediaXmlns = nullptr;
    const char *snapshotAction = nullptr;
    const char *streamAction = nullptr;

    auto tryProfiles = [&](const char *xaddr, bool media2) -> bool {
        if (xaddr == nullptr || xaddr[0] == '\0' || !looksLikeUri(xaddr)) {
            return false;
        }
        const char *body = media2
            ? "<tr2:GetProfiles xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\"/>"
            : "<trt:GetProfiles xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\"/>";
        const char *action = media2
            ? "http://www.onvif.org/ver20/media/wsdl/GetProfiles"
            : "http://www.onvif.org/ver10/media/wsdl/GetProfiles";
        char *profilesXml = soapCall(
            xaddr,
            username,
            password,
            authMode,
            action,
            body,
            nullptr,
            &status
        );
        if (profilesXml == nullptr) {
            return false;
        }
        if (xmlHasFault(profilesXml)) {
            const bool unauthorized = xmlLooksUnauthorized(profilesXml);
            free(profilesXml);
            if (unauthorized) {
                setError(errorText, errorMax, "ONVIF unauthorized");
            }
            return false;
        }
        profileCount = collectProfileTokens(profilesXml, profileTokens, CAMERA_CHANNELS);
        free(profilesXml);
        if (profileCount <= 0) {
            return false;
        }
        usedMediaXaddr = xaddr;
        usedMediaXmlns = media2 ? "ver20/media" : "ver10/media";
        snapshotAction = media2
            ? "http://www.onvif.org/ver20/media/wsdl/GetSnapshotUri"
            : "http://www.onvif.org/ver10/media/wsdl/GetSnapshotUri";
        streamAction = media2
            ? "http://www.onvif.org/ver20/media/wsdl/GetStreamUri"
            : "http://www.onvif.org/ver10/media/wsdl/GetStreamUri";
        return true;
    };

    // Prefer classic Media, then Media2, then device URL.
    if (!tryProfiles(mediaXaddr, false)
        && !tryProfiles(media2Xaddr, true)
        && !tryProfiles(deviceServiceUrl, false)
        && !tryProfiles(deviceServiceUrl, true)) {
        if (errorText != nullptr && errorMax > 0 && errorText[0] == '\0') {
            setError(errorText, errorMax, "no media profiles");
        }
        return false;
    }

    const bool anyUrl = fillUrisForProfiles(
        usedMediaXaddr,
        username,
        password,
        authMode,
        usedMediaXmlns,
        snapshotAction,
        streamAction,
        profileTokens,
        profileCount,
        out
    );
    if (!anyUrl) {
        setError(errorText, errorMax, "ONVIF returned no media URLs");
        return false;
    }
    if (out->snapshotUrl[0][0] != '\0') {
        strncpy(out->url, out->snapshotUrl[0], sizeof(out->url) - 1);
    } else if (out->liveUrl[0][0] != '\0') {
        strncpy(out->url, out->liveUrl[0], sizeof(out->url) - 1);
    }
    if (out->name[0] == '\0') {
        strncpy(out->name, out->host, sizeof(out->name) - 1);
    }
    setError(errorText, errorMax, "");
    return true;
}

}  // namespace

bool onvifResolveMedia(
    const char *deviceServiceUrl,
    const char *username,
    const char *password,
    OnvifResolvedMedia *out,
    char *errorText,
    size_t errorMax
) {
    if (out == nullptr) {
        setError(errorText, errorMax, "bad args");
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->port = 80;
    if (deviceServiceUrl == nullptr || deviceServiceUrl[0] == '\0') {
        setError(errorText, errorMax, "missing device URL");
        return false;
    }
    if (!parseHttpUrl(deviceServiceUrl, out->host, sizeof(out->host), &out->port)) {
        setError(errorText, errorMax, "bad device URL");
        return false;
    }
    strncpy(out->url, deviceServiceUrl, sizeof(out->url) - 1);

    const bool haveUser = username != nullptr && username[0] != '\0';
    OnvifAuthMode modes[3];
    int modeCount = 0;
    if (!haveUser) {
        modes[modeCount++] = OnvifAuthNone;
    } else {
        modes[modeCount++] = OnvifAuthPasswordDigest;
        modes[modeCount++] = OnvifAuthPasswordText;
        modes[modeCount++] = OnvifAuthNone;
    }

    char lastError[CAMERA_STATUS_ERROR_MAX];
    lastError[0] = '\0';
    for (int modeIndex = 0; modeIndex < modeCount; modeIndex++) {
        OnvifResolvedMedia attempt;
        memset(&attempt, 0, sizeof(attempt));
        strncpy(attempt.host, out->host, sizeof(attempt.host) - 1);
        attempt.port = out->port;
        strncpy(attempt.url, out->url, sizeof(attempt.url) - 1);
        char attemptError[CAMERA_STATUS_ERROR_MAX];
        attemptError[0] = '\0';
        if (resolveWithAuth(
                deviceServiceUrl,
                username,
                password,
                modes[modeIndex],
                &attempt,
                attemptError,
                sizeof(attemptError)
            )) {
            *out = attempt;
            setError(errorText, errorMax, "");
            LOGGER.info(String("ONVIF media OK for ") + out->host);
            return true;
        }
        strncpy(lastError, attemptError, sizeof(lastError) - 1);
        // Only retry other auth modes on auth failures; hard transport errors stop early.
        if (strcmp(attemptError, "GetCapabilities failed") == 0) {
            break;
        }
        if (strcmp(attemptError, "ONVIF unauthorized") != 0 && modeIndex + 1 < modeCount) {
            // keep trying other modes for profile/url failures too when creds exist
        }
        yield();
    }

    setError(errorText, errorMax, lastError[0] != '\0' ? lastError : "ONVIF resolve failed");
    LOGGER.warning(
        String("ONVIF media resolve failed for ") + out->host + ": "
        + (errorText != nullptr ? errorText : "")
    );
    return false;
}
