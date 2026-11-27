#include "HostHttpServer.h"

#if defined(BOARD_ROLE_HOST)

#include "Defines.h"
#include "HostAuth.h"
#include "HostProductApi.h"
#include "HostServices.h"
#include "generated/EmbeddedWebAssetsIdf.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *kLogTag = "host_http";
static httpd_handle_t s_server = nullptr;
static constexpr int kListenPort = 80;
static constexpr int kBindRetries = 5;

static esp_err_t sendGzipAsset(
    httpd_req_t *request,
    const uint8_t *payload,
    size_t payloadLen,
    const char *mime
) {
    httpd_resp_set_type(request, mime);
    httpd_resp_set_hdr(request, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(request, "Cache-Control", "no-cache");
    return httpd_resp_send(request, reinterpret_cast<const char *>(payload), payloadLen);
}

static esp_err_t handleRootGet(httpd_req_t *request) {
    return sendGzipAsset(
        request,
        kEmbeddedIndexHtmlGz,
        kEmbeddedIndexHtmlGzLen,
        kEmbeddedIndexHtmlGzMime
    );
}

static esp_err_t handleUserGet(httpd_req_t *request) {
    return sendGzipAsset(
        request,
        kEmbeddedUserHtmlGz,
        kEmbeddedUserHtmlGzLen,
        kEmbeddedUserHtmlGzMime
    );
}

static esp_err_t handleCssGet(httpd_req_t *request) {
    return sendGzipAsset(
        request,
        kEmbeddedAllCssGz,
        kEmbeddedAllCssGzLen,
        kEmbeddedAllCssGzMime
    );
}

static bool extractJsonString(const char *json, const char *key, char *out, size_t outSize) {
    if (json == nullptr || key == nullptr || out == nullptr || outSize == 0) {
        return false;
    }
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *found = strstr(json, pattern);
    if (found == nullptr) {
        return false;
    }
    found = strchr(found + strlen(pattern), ':');
    if (found == nullptr) {
        return false;
    }
    found++;
    while (*found == ' ' || *found == '\t') {
        found++;
    }
    if (*found != '"') {
        return false;
    }
    found++;
    size_t index = 0;
    while (*found != '\0' && *found != '"' && index + 1 < outSize) {
        if (*found == '\\' && found[1] != '\0') {
            found++;
        }
        out[index++] = *found++;
    }
    out[index] = '\0';
    return true;
}

static bool extractJsonBool(const char *json, const char *key, bool *out) {
    if (json == nullptr || key == nullptr || out == nullptr) {
        return false;
    }
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *found = strstr(json, pattern);
    if (found == nullptr) {
        return false;
    }
    found = strchr(found + strlen(pattern), ':');
    if (found == nullptr) {
        return false;
    }
    found++;
    while (*found == ' ' || *found == '\t') {
        found++;
    }
    if (strncmp(found, "true", 4) == 0) {
        *out = true;
        return true;
    }
    if (strncmp(found, "false", 5) == 0) {
        *out = false;
        return true;
    }
    return false;
}

static esp_err_t handleAuthLoginPost(httpd_req_t *request) {
    char body[512];
    const int received = httpd_req_recv(request, body, sizeof(body) - 1);
    if (received <= 0) {
        httpd_resp_set_status(request, "400 Bad Request");
        return httpd_resp_send(request, "Bad body", HTTPD_RESP_USE_STRLEN);
    }
    body[received] = '\0';

    char userName[USER_NAME_MAX] = {};
    char password[64] = {};
    bool rememberMe = false;
    extractJsonString(body, "userName", userName, sizeof(userName));
    extractJsonString(body, "password", password, sizeof(password));
    extractJsonBool(body, "rememberMe", &rememberMe);

    char tokenHex[AUTH_SESSION_COOKIE_MAX] = {};
    uint32_t maxAgeSec = 0;
    if (!hostAuthLogin(userName, password, rememberMe, tokenHex, sizeof(tokenHex), &maxAgeSec)) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return httpd_resp_send(request, "Invalid user name or password", HTTPD_RESP_USE_STRLEN);
    }

    char cookie[192];
    snprintf(
        cookie,
        sizeof(cookie),
        "%s=%s; Path=/; HttpOnly; Max-Age=%u",
        AUTH_COOKIE_NAME,
        tokenHex,
        (unsigned)maxAgeSec
    );
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Set-Cookie", cookie);
    return httpd_resp_send(request, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handleAuthMeGet(httpd_req_t *request) {
    size_t cookieLen = httpd_req_get_hdr_value_len(request, "Cookie") + 1;
    char *cookieHeader = nullptr;
    if (cookieLen > 1) {
        cookieHeader = static_cast<char *>(malloc(cookieLen));
        if (cookieHeader != nullptr) {
            if (httpd_req_get_hdr_value_str(request, "Cookie", cookieHeader, cookieLen) != ESP_OK) {
                free(cookieHeader);
                cookieHeader = nullptr;
            }
        }
    }

    char userName[USER_NAME_MAX] = {};
    bool isAdmin = false;
    const bool ok = hostAuthSessionUser(cookieHeader, userName, sizeof(userName), &isAdmin);
    free(cookieHeader);
    if (!ok) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return httpd_resp_send(request, "Unauthorized", HTTPD_RESP_USE_STRLEN);
    }

    char json[256];
    snprintf(
        json,
        sizeof(json),
        "{\"userName\":\"%s\",\"addedAt\":0,\"isAdmin\":%s,\"editDevices\":false,"
        "\"addDevices\":false,\"removeDevices\":false,\"editUsers\":false,"
        "\"editConsoles\":false,\"editScenes\":false,\"monitor\":false,"
        "\"controlDevices\":false,\"isBlocked\":false,\"theme\":\"dark\",\"consoles\":[]}",
        userName,
        isAdmin ? "true" : "false"
    );
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handleAuthLogoutPost(httpd_req_t *request) {
    size_t cookieLen = httpd_req_get_hdr_value_len(request, "Cookie") + 1;
    if (cookieLen > 1) {
        char *cookieHeader = static_cast<char *>(malloc(cookieLen));
        if (cookieHeader != nullptr
            && httpd_req_get_hdr_value_str(request, "Cookie", cookieHeader, cookieLen) == ESP_OK) {
            char tokenHex[AUTH_SESSION_COOKIE_MAX] = {};
            const char *found = strstr(cookieHeader, AUTH_COOKIE_NAME "=");
            if (found != nullptr) {
                found += strlen(AUTH_COOKIE_NAME "=");
                size_t index = 0;
                while (index + 1 < sizeof(tokenHex) && found[index] != '\0' && found[index] != ';') {
                    tokenHex[index] = found[index];
                    index++;
                }
                tokenHex[index] = '\0';
                hostAuthLogout(tokenHex);
            }
        }
        free(cookieHeader);
    }
    char clearCookie[64];
    snprintf(clearCookie, sizeof(clearCookie), "%s=; Path=/; HttpOnly; Max-Age=0", AUTH_COOKIE_NAME);
    httpd_resp_set_hdr(request, "Set-Cookie", clearCookie);
    return httpd_resp_send(request, "Signed out", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handleLargeStatusGet(httpd_req_t *request) {
    const size_t bytes = 8192;
    char *json = static_cast<char *>(hostPsramAlloc(bytes));
    if (json == nullptr) {
        httpd_resp_set_status(request, "507 Insufficient Storage");
        return httpd_resp_send(request, "Out of memory", HTTPD_RESP_USE_STRLEN);
    }
    snprintf(
        json,
        bytes,
        "{\"version\":\"%s\",\"mqttClient\":%s,\"mqttBroker\":%s,\"mqttConnected\":%s,"
        "\"spiHealthy\":%s,\"spiIdle\":%s,\"smtpDepth\":%u,\"interconnect\":%s,\"pad\":\"",
        FIRMWARE_VERSION,
        hostMqttClientEnabled() ? "true" : "false",
        hostMqttBrokerEnabled() ? "true" : "false",
        hostMqttClientConnected() ? "true" : "false",
        hostSpiIsLinkHealthy() ? "true" : "false",
        hostSpiIsOutboundIdle() ? "true" : "false",
        (unsigned)hostSmtpQueueDepth(),
        hostInterconnectIsListening() ? "true" : "false"
    );
    const size_t prefixLen = strlen(json);
    for (size_t i = prefixLen; i + 2 < bytes; ++i) {
        json[i] = 'x';
    }
    json[bytes - 2] = '"';
    json[bytes - 1] = '}';
    // Ensure null within buffer for logging; send exact length.
    httpd_resp_set_type(request, "application/json");
    const esp_err_t err = httpd_resp_send(request, json, bytes);
    hostPsramFree(json);
    return err;
}

static esp_err_t handleGatewayStatusGet(httpd_req_t *request) {
    char json[384];
    snprintf(
        json,
        sizeof(json),
        "{\"version\":\"%s\",\"mqttClient\":%s,\"mqttBroker\":%s,\"mqttConnected\":%s,"
        "\"spiHealthy\":%s,\"spiIdle\":%s,\"smtpDepth\":%u,\"interconnect\":%s}",
        FIRMWARE_VERSION,
        hostMqttClientEnabled() ? "true" : "false",
        hostMqttBrokerEnabled() ? "true" : "false",
        hostMqttClientConnected() ? "true" : "false",
        hostSpiIsLinkHealthy() ? "true" : "false",
        hostSpiIsOutboundIdle() ? "true" : "false",
        (unsigned)hostSmtpQueueDepth(),
        hostInterconnectIsListening() ? "true" : "false"
    );
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, json, HTTPD_RESP_USE_STRLEN);
}

static void registerUri(httpd_handle_t server, const char *uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t *)) {
    httpd_uri_t route = {};
    route.uri = uri;
    route.method = method;
    route.handler = handler;
    route.user_ctx = nullptr;
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route));
}

static bool startServerOnce(void) {
    if (s_server != nullptr) {
        return true;
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = kListenPort;
    config.lru_purge_enable = true;
    config.max_uri_handlers = 48;
    // Keep HTTP workers off CPU1 (SMTP core isolation intent from smtp-send).
    config.core_id = 0;

    const esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGW(kLogTag, "httpd_start failed: %s", esp_err_to_name(err));
        s_server = nullptr;
        return false;
    }

    registerUri(s_server, "/", HTTP_GET, handleRootGet);
    registerUri(s_server, "/index.html", HTTP_GET, handleRootGet);
    registerUri(s_server, "/user.html", HTTP_GET, handleUserGet);
    registerUri(s_server, "/css/all.css", HTTP_GET, handleCssGet);
    registerUri(s_server, "/api/auth/login", HTTP_POST, handleAuthLoginPost);
    registerUri(s_server, "/api/auth/me", HTTP_GET, handleAuthMeGet);
    registerUri(s_server, "/api/auth/logout", HTTP_POST, handleAuthLogoutPost);
    registerUri(s_server, "/api/status/large", HTTP_GET, handleLargeStatusGet);
    registerUri(s_server, "/api/gateway/status", HTTP_GET, handleGatewayStatusGet);
    hostProductRegisterRoutes(s_server);

    ESP_LOGI(kLogTag, "HTTP console listening on port %d", kListenPort);
    return true;
}

bool hostHttpBegin(void) {
    hostAuthBegin();
    return startServerOnce();
}

void hostHttpEnd(void) {
    if (s_server == nullptr) {
        return;
    }
    httpd_stop(s_server);
    s_server = nullptr;
    ESP_LOGI(kLogTag, "HTTP console stopped");
}

bool hostHttpRebind(void) {
    ESP_LOGI(kLogTag, "Rebinding web console");
    hostHttpEnd();
    for (int attempt = 0; attempt < kBindRetries; ++attempt) {
        vTaskDelay(pdMS_TO_TICKS(50u * (uint32_t)(attempt + 1)));
        if (startServerOnce()) {
            ESP_LOGI(kLogTag, "Web console rebound on port %d", kListenPort);
            return true;
        }
        ESP_LOGW(kLogTag, "Bind retry %d/%d failed", attempt + 1, kBindRetries);
    }
    ESP_LOGE(kLogTag, "Web console rebind failed on port %d", kListenPort);
    return false;
}

bool hostHttpIsRunning(void) {
    return s_server != nullptr;
}

#endif
