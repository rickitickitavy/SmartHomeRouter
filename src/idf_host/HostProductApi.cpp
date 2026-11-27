#include "HostProductApi.h"

#if defined(BOARD_ROLE_HOST)

#include "AutomationStore.h"
#include "CameraStore.h"
#include "ConsoleStore.h"
#include "Defines.h"
#include "HostAuth.h"
#include "HostServices.h"
#include "InterconnectStore.h"
#include "JsonField.h"
#include "Logger.h"
#include "PsramJsonBuffer.h"
#include "SceneStore.h"
#include "SettingsManager.h"
#include "UserStore.h"

#include "esp_http_server.h"
#include "esp_log.h"

#include <stdlib.h>
#include <string.h>

static const char *kLogTag = "host_product";

static SettingsManager *s_settings = nullptr;
static UserStore *s_users = nullptr;
static ConsoleStore *s_consoles = nullptr;
static AutomationStore *s_automations = nullptr;
static SceneStore *s_scenes = nullptr;
static CameraStore *s_cameras = nullptr;
static InterconnectStore *s_interconnect = nullptr;

static esp_err_t sendText(httpd_req_t *request, const char *status, const char *type, const char *body) {
    httpd_resp_set_status(request, status);
    httpd_resp_set_type(request, type);
    return httpd_resp_send(request, body, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t sendJsonBuffer(httpd_req_t *request, const PsramJsonBuffer &json) {
    if (!json.ok()) {
        return sendText(request, "507 Insufficient Storage", "text/plain", "Out of memory");
    }
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, json.c_str(), json.length());
}

static bool readCookieHeader(httpd_req_t *request, char **outHeader) {
    *outHeader = nullptr;
    const size_t cookieLen = httpd_req_get_hdr_value_len(request, "Cookie") + 1;
    if (cookieLen <= 1) {
        return false;
    }
    char *cookieHeader = static_cast<char *>(malloc(cookieLen));
    if (cookieHeader == nullptr) {
        return false;
    }
    if (httpd_req_get_hdr_value_str(request, "Cookie", cookieHeader, cookieLen) != ESP_OK) {
        free(cookieHeader);
        return false;
    }
    *outHeader = cookieHeader;
    return true;
}

static bool requireSignedIn(httpd_req_t *request, bool requireAdmin) {
    char *cookieHeader = nullptr;
    if (!readCookieHeader(request, &cookieHeader)) {
        sendText(request, "401 Unauthorized", "text/plain", "Unauthorized");
        return false;
    }
    char userName[USER_NAME_MAX] = {};
    bool isAdmin = false;
    const bool ok = hostAuthSessionUser(cookieHeader, userName, sizeof(userName), &isAdmin);
    free(cookieHeader);
    if (!ok) {
        sendText(request, "401 Unauthorized", "text/plain", "Unauthorized");
        return false;
    }
    if (requireAdmin && !isAdmin) {
        sendText(request, "403 Forbidden", "text/plain", "Forbidden");
        return false;
    }
    return true;
}

static bool readBodyPsram(httpd_req_t *request, PsramJsonBuffer *out) {
    out->clear();
    out->requireExternalMemory();
    const size_t total = request->content_len;
    if (total == 0) {
        return true;
    }
    if (!out->reserve(total + 1)) {
        sendText(request, "507 Insufficient Storage", "text/plain", "Out of memory");
        return false;
    }
    size_t received = 0;
    while (received < total) {
        char chunk[512];
        const size_t want = (total - received) < sizeof(chunk) ? (total - received) : sizeof(chunk);
        const int got = httpd_req_recv(request, chunk, want);
        if (got <= 0) {
            sendText(request, "400 Bad Request", "text/plain", "Bad body");
            return false;
        }
        if (!out->appendBytes(chunk, (size_t)got)) {
            sendText(request, "507 Insufficient Storage", "text/plain", "Out of memory");
            return false;
        }
        received += (size_t)got;
    }
    return true;
}

static esp_err_t handleConsolesGet(httpd_req_t *request) {
    if (!requireSignedIn(request, false) || s_consoles == nullptr || s_users == nullptr) {
        return ESP_OK;
    }
    return sendJsonBuffer(request, s_consoles->listSummaryJson(s_users));
}

static esp_err_t handleConsolesStoreGet(httpd_req_t *request) {
    if (!requireSignedIn(request, true) || s_consoles == nullptr) {
        return ESP_OK;
    }
    return sendJsonBuffer(request, s_consoles->listFullJson());
}

static esp_err_t handleConsolesMineGet(httpd_req_t *request) {
    if (!requireSignedIn(request, false) || s_consoles == nullptr) {
        return ESP_OK;
    }
    // Factory auth has no UserRecord pointer; mine list is empty until auth uses UserStore.
    return sendText(request, "200 OK", "application/json", "[]");
}

static esp_err_t handleConsolesRestorePost(httpd_req_t *request) {
    PsramJsonBuffer body;
    if (!readBodyPsram(request, &body)) {
        return ESP_OK;
    }
    if (!requireSignedIn(request, true) || s_consoles == nullptr) {
        return ESP_OK;
    }
    body.trim();
    if (body.length() == 0 || body.charAt(0) != '[') {
        return sendText(request, "400 Bad Request", "text/plain", "Need a consoles JSON array");
    }
    if (!s_consoles->replaceAllFromJson(body.c_str())) {
        return sendText(request, "400 Bad Request", "text/plain", "Need a valid consoles list");
    }
    return sendText(request, "200 OK", "text/plain", "Restored");
}

static esp_err_t handleAutomationsGet(httpd_req_t *request) {
    if (!requireSignedIn(request, false) || s_automations == nullptr) {
        return ESP_OK;
    }
    return sendJsonBuffer(request, s_automations->listSummaryJson());
}

static esp_err_t handleAutomationsStoreGet(httpd_req_t *request) {
    if (!requireSignedIn(request, true) || s_automations == nullptr) {
        return ESP_OK;
    }
    return sendJsonBuffer(request, s_automations->listFullJson());
}

static esp_err_t handleAutomationsRestorePost(httpd_req_t *request) {
    PsramJsonBuffer body;
    if (!readBodyPsram(request, &body)) {
        return ESP_OK;
    }
    if (!requireSignedIn(request, true) || s_automations == nullptr) {
        return ESP_OK;
    }
    body.trim();
    char query[64] = {};
    bool merge = false;
    if (httpd_req_get_url_query_str(request, query, sizeof(query)) == ESP_OK) {
        merge = strstr(query, "merge=1") != nullptr;
    }
    const bool ok = merge ? s_automations->mergeAllFromJson(body.c_str())
                          : s_automations->replaceAllFromJson(body.c_str());
    if (!ok) {
        return sendText(request, "400 Bad Request", "text/plain", "Need a valid automations list");
    }
    return sendText(request, "200 OK", "text/plain", "Restored");
}

static esp_err_t handleScenesGet(httpd_req_t *request) {
    if (!requireSignedIn(request, false) || s_scenes == nullptr) {
        return ESP_OK;
    }
    return sendJsonBuffer(request, s_scenes->listJson());
}

static esp_err_t handleScenesStoreGet(httpd_req_t *request) {
    if (!requireSignedIn(request, true) || s_scenes == nullptr) {
        return ESP_OK;
    }
    return sendJsonBuffer(request, s_scenes->exportJson());
}

static esp_err_t handleScenesRestorePost(httpd_req_t *request) {
    PsramJsonBuffer body;
    if (!readBodyPsram(request, &body)) {
        return ESP_OK;
    }
    if (!requireSignedIn(request, true) || s_scenes == nullptr) {
        return ESP_OK;
    }
    if (s_scenes->replaceAllFromJson(body.c_str()) != SceneWriteOk) {
        return sendText(request, "400 Bad Request", "text/plain", "Need a valid scenes list");
    }
    return sendText(request, "200 OK", "text/plain", "Restored");
}

static esp_err_t handleCamerasGet(httpd_req_t *request) {
    if (!requireSignedIn(request, false) || s_cameras == nullptr) {
        return ESP_OK;
    }
    return sendJsonBuffer(request, s_cameras->listJson(false));
}

static esp_err_t handleCamerasStoreGet(httpd_req_t *request) {
    if (!requireSignedIn(request, true) || s_cameras == nullptr) {
        return ESP_OK;
    }
    return sendJsonBuffer(request, s_cameras->listJson(true));
}

static esp_err_t handleCamerasRestorePost(httpd_req_t *request) {
    PsramJsonBuffer body;
    if (!readBodyPsram(request, &body)) {
        return ESP_OK;
    }
    if (!requireSignedIn(request, true) || s_cameras == nullptr) {
        return ESP_OK;
    }
    if (s_cameras->replaceAllFromJson(body.c_str()) != CameraWriteOk) {
        return sendText(request, "400 Bad Request", "text/plain", "Need a valid cameras list");
    }
    return sendText(request, "200 OK", "text/plain", "Restored");
}

static esp_err_t handleDevicesGet(httpd_req_t *request) {
    if (!requireSignedIn(request, false) || s_settings == nullptr) {
        return ESP_OK;
    }
    return sendJsonBuffer(request, s_settings->deviceMap()->listJson());
}

static esp_err_t handleDevicesStoreGet(httpd_req_t *request) {
    if (!requireSignedIn(request, true) || s_settings == nullptr) {
        return ESP_OK;
    }
    return sendJsonBuffer(request, s_settings->deviceMap()->listStoreJson());
}

static esp_err_t handleHistoryGet(httpd_req_t *request) {
    if (!requireSignedIn(request, false)) {
        return ESP_OK;
    }
    // History ring port lands with SettingsManager/SD soak; empty series keeps UI alive.
    return sendText(
        request,
        "200 OK",
        "application/json",
        "{\"ok\":true,\"points\":[],\"series\":[],\"truncated\":true}"
    );
}

static void appendMqttJson(PsramJsonBuffer &json) {
    GlobalSettings *settings = s_settings->getSettings();
    char digits[24];
    json += "\"serverType\":\"";
    json += mqttServerTypeName(settings->mqtt.serverType);
    json += "\",\"server\":\"";
    appendJsonEscaped(json, settings->mqtt.server, sizeof(settings->mqtt.server));
    json += "\",\"port\":";
    snprintf(digits, sizeof(digits), "%d", settings->mqtt.port);
    json += digits;
    json += ",\"username\":\"";
    appendJsonEscaped(json, settings->mqtt.username, sizeof(settings->mqtt.username));
    json += "\",\"password\":\"";
    appendJsonEscaped(json, settings->mqtt.password, sizeof(settings->mqtt.password));
    json += "\",\"baseTopic\":\"";
    appendJsonEscaped(json, settings->mqtt.baseTopic, sizeof(settings->mqtt.baseTopic));
    json += "\",\"reconnectIntervalMs\":";
    snprintf(digits, sizeof(digits), "%ld", (long)settings->mqtt.reconnectIntervalMs);
    json += digits;
    json += ",\"clientTimeoutMs\":";
    snprintf(digits, sizeof(digits), "%d", settings->mqtt.clientTimeoutMs);
    json += digits;
    json += ",\"serverBornTopic\":\"";
    appendJsonEscaped(json, settings->mqtt.serverBornTopic, sizeof(settings->mqtt.serverBornTopic));
    json += "\",\"bornIntervalMin\":";
    snprintf(digits, sizeof(digits), "%u", (unsigned)settings->mqtt.bornIntervalMin);
    json += digits;
}

static void appendEmailJson(PsramJsonBuffer &json, bool exportWithEmptyPassword) {
    GlobalSettings *settings = s_settings->getSettings();
    char digits[24];
    json += "\"host\":\"";
    appendJsonEscaped(json, settings->email.host, sizeof(settings->email.host));
    json += "\",\"port\":";
    snprintf(digits, sizeof(digits), "%d", settings->email.port);
    json += digits;
    json += ",\"encryption\":\"";
    json += emailEncryptionModeName(settings->email.encryption);
    json += "\",\"authEnabled\":";
    json += settings->email.authEnabled ? "true" : "false";
    json += ",\"username\":\"";
    appendJsonEscaped(json, settings->email.username, sizeof(settings->email.username));
    if (exportWithEmptyPassword) {
        json += "\",\"password\":\"\"";
    } else {
        json += "\",\"passwordConfigured\":";
        json += settings->email.password[0] != '\0' ? "true" : "false";
    }
    json += ",\"fromAddress\":\"";
    appendJsonEscaped(json, settings->email.fromAddress, sizeof(settings->email.fromAddress));
    json += "\",\"fromName\":\"";
    appendJsonEscaped(json, settings->email.fromName, sizeof(settings->email.fromName));
    json += "\",\"replyTo\":\"";
    appendJsonEscaped(json, settings->email.replyTo, sizeof(settings->email.replyTo));
    json += "\",\"ehloHostname\":\"";
    appendJsonEscaped(json, settings->email.ehloHostname, sizeof(settings->email.ehloHostname));
    json += "\",\"connectTimeoutMs\":";
    snprintf(digits, sizeof(digits), "%d", settings->email.connectTimeoutMs);
    json += digits;
    json += ",\"sendTimeoutMs\":";
    snprintf(digits, sizeof(digits), "%d", settings->email.sendTimeoutMs);
    json += digits;
    json += ",\"checkAccountIntervalMin\":";
    snprintf(digits, sizeof(digits), "%d", settings->email.checkAccountIntervalMin);
    json += digits;
    json += ",\"configured\":";
    json += SettingsManager::isEmailConfigured(settings->email) ? "true" : "false";
}

static esp_err_t handleSettingsExportGet(httpd_req_t *request) {
    if (!requireSignedIn(request, true) || s_settings == nullptr) {
        return ESP_OK;
    }
    PsramJsonBuffer camerasJson =
        s_cameras != nullptr ? s_cameras->listJson(true) : PsramJsonBuffer();
    if (s_cameras == nullptr) {
        camerasJson += "[]";
    }
    PsramJsonBuffer scenesJson = s_scenes != nullptr ? s_scenes->exportJson() : PsramJsonBuffer();
    if (s_scenes == nullptr) {
        scenesJson += "[]";
    }
    PsramJsonBuffer devicesJson = s_settings->deviceMap()->listStoreJson();
    PsramJsonBuffer usersJson = s_users != nullptr ? s_users->listExportJson() : PsramJsonBuffer();
    if (s_users == nullptr) {
        usersJson += "[]";
    }
    PsramJsonBuffer consolesJson = s_consoles != nullptr ? s_consoles->listFullJson() : PsramJsonBuffer();
    if (s_consoles == nullptr) {
        consolesJson += "[]";
    }
    PsramJsonBuffer automationsJson =
        s_automations != nullptr ? s_automations->listFullJson() : PsramJsonBuffer();
    if (s_automations == nullptr) {
        automationsJson += "[]";
    }
    if (!camerasJson.ok() || !scenesJson.ok() || !devicesJson.ok() || !usersJson.ok()
        || !consolesJson.ok() || !automationsJson.ok()) {
        return sendText(request, "507 Insufficient Storage", "text/plain", "Out of memory");
    }

    GlobalSettings *settings = s_settings->getSettings();
    PsramJsonBuffer json;
    json.requireExternalMemory();
    char number[24];
    json += "{\"version\":\"";
    json += FIRMWARE_VERSION;
    json += "\",\"mqtt\":{";
    appendMqttJson(json);
    json += "},\"email\":{";
    appendEmailJson(json, true);
    json += "},\"zigbee\":{\"channel\":";
    snprintf(number, sizeof(number), "%u", (unsigned)settings->zigbee.channel);
    json += number;
    json += ",\"permitJoinOnBootSec\":";
    snprintf(number, sizeof(number), "%u", (unsigned)settings->zigbee.permitJoinOnBootSec);
    json += number;
    json += "},\"hardware\":{\"spiSpeedHz\":";
    snprintf(number, sizeof(number), "%lu", (unsigned long)s_settings->spiSpeedHz());
    json += number;
    json += ",\"lowMemoryThresholdKb\":";
    snprintf(number, sizeof(number), "%u", (unsigned)s_settings->lowMemoryThresholdKb());
    json += number;
    json += ",\"timeZoneUtcOffsetMin\":";
    snprintf(number, sizeof(number), "%d", (int)s_settings->timeZoneUtcOffsetMin());
    json += number;
    json += "},\"ui\":{\"theme\":\"dark\"},\"devices\":";
    json += devicesJson.c_str();
    json += ",\"users\":";
    json += usersJson.c_str();
    json += ",\"consoles\":";
    json += consolesJson.c_str();
    json += ",\"automations\":";
    json += automationsJson.c_str();
    json += ",\"cameras\":";
    json += camerasJson.c_str();
    json += ",\"scenes\":";
    json += scenesJson.c_str();
    if (s_interconnect != nullptr) {
        PsramJsonBuffer interconnectJson = s_interconnect->exportJson();
        if (!interconnectJson.ok()) {
            return sendText(request, "507 Insufficient Storage", "text/plain", "Out of memory");
        }
        json += ",\"interconnect\":";
        json += interconnectJson.c_str();
    }
    json += "}";
    // Wi-Fi credentials intentionally excluded.
    return sendJsonBuffer(request, json);
}

static esp_err_t handleSettingsRestorePost(httpd_req_t *request) {
    PsramJsonBuffer body;
    if (!readBodyPsram(request, &body)) {
        return ESP_OK;
    }
    if (!requireSignedIn(request, true)) {
        return ESP_OK;
    }

    PsramJsonBuffer devicesJson;
    PsramJsonBuffer usersJson;
    PsramJsonBuffer consolesJson;
    PsramJsonBuffer automationsJson;
    PsramJsonBuffer camerasJson;
    PsramJsonBuffer scenesJson;
    String interconnectJson;
    const bool haveDevices = extractJsonKeyedSlice(body.c_str(), "devices", '[', devicesJson);
    const bool haveUsers = extractJsonKeyedSlice(body.c_str(), "users", '[', usersJson);
    const bool haveConsoles = extractJsonKeyedSlice(body.c_str(), "consoles", '[', consolesJson);
    const bool haveAutomations = extractJsonKeyedSlice(body.c_str(), "automations", '[', automationsJson);
    const bool haveCameras = extractJsonKeyedSlice(body.c_str(), "cameras", '[', camerasJson);
    const bool haveScenes = extractJsonKeyedSlice(body.c_str(), "scenes", '[', scenesJson);
    const bool haveInterconnect = extractJsonKeyedSlice(body.c_str(), "interconnect", '{', interconnectJson);

    if (!devicesJson.ok() || !usersJson.ok() || !consolesJson.ok() || !automationsJson.ok()
        || !camerasJson.ok() || !scenesJson.ok()) {
        return sendText(request, "507 Insufficient Storage", "text/plain", "Out of memory");
    }

    if (haveUsers && s_users != nullptr) {
        char removed[USER_STORE_MAX][USER_NAME_MAX];
        int removedCount = 0;
        if (!s_users->replaceFromExportJson(usersJson.c_str(), removed, &removedCount, USER_STORE_MAX)) {
            return sendText(
                request,
                "400 Bad Request",
                "text/plain",
                "Need a valid users list with at least one unlocked admin"
            );
        }
    }
    if (haveConsoles && (s_consoles == nullptr || !s_consoles->replaceAllFromJson(consolesJson.c_str()))) {
        return sendText(request, "400 Bad Request", "text/plain", "Need a valid consoles list");
    }
    if (haveAutomations
        && (s_automations == nullptr || !s_automations->replaceAllFromJson(automationsJson.c_str()))) {
        return sendText(request, "400 Bad Request", "text/plain", "Need a valid automations list");
    }
    if (haveCameras && (s_cameras == nullptr || s_cameras->replaceAllFromJson(camerasJson.c_str()) != CameraWriteOk)) {
        return sendText(request, "400 Bad Request", "text/plain", "Need a valid cameras list");
    }
    if (haveScenes
        && (s_scenes == nullptr || s_scenes->replaceAllFromJson(scenesJson.c_str()) != SceneWriteOk)) {
        return sendText(request, "400 Bad Request", "text/plain", "Need a valid scenes list");
    }
    if (haveDevices && s_settings != nullptr) {
        s_settings->deviceMap()->replaceFromJson(devicesJson.c_str());
        s_settings->saveDevicesJson();
    }
    if (haveInterconnect && s_interconnect != nullptr) {
        if (!s_interconnect->importJson(interconnectJson.c_str(), false)) {
            return sendText(request, "400 Bad Request", "text/plain", "Need a valid interconnect object");
        }
    }
    return sendText(request, "200 OK", "text/plain", "Restored");
}

static void registerUri(httpd_handle_t server, const char *uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t *)) {
    httpd_uri_t route = {};
    route.uri = uri;
    route.method = method;
    route.handler = handler;
    route.user_ctx = nullptr;
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route));
}

bool hostProductBegin(void) {
    LOGGER.begin();
    LOGGER.setRoleLabel("host");

    s_settings = new SettingsManager();
    s_settings->readSettings();
    s_settings->loadDeviceFile();

    s_users = new UserStore();
    s_users->begin(true);

    s_consoles = new ConsoleStore();
    s_consoles->begin(true);

    s_automations = new AutomationStore();
    s_automations->setDeviceMap(s_settings->deviceMap());
    s_automations->begin(true);

    s_scenes = new SceneStore();
    s_scenes->begin(true);

    s_cameras = new CameraStore();
    s_cameras->begin(true);

    s_interconnect = new InterconnectStore();
    s_interconnect->begin(true);

    hostFailsafeSetThresholdKb(s_settings->lowMemoryThresholdKb());
    ESP_LOGI(kLogTag, "Product stores ready");
    return true;
}

void hostProductRegisterRoutes(httpd_handle_t server) {
    if (server == nullptr) {
        return;
    }
    registerUri(server, "/api/consoles", HTTP_GET, handleConsolesGet);
    registerUri(server, "/api/consoles/store", HTTP_GET, handleConsolesStoreGet);
    registerUri(server, "/api/consoles/mine", HTTP_GET, handleConsolesMineGet);
    registerUri(server, "/api/consoles/restore", HTTP_POST, handleConsolesRestorePost);
    registerUri(server, "/api/automations", HTTP_GET, handleAutomationsGet);
    registerUri(server, "/api/automations/store", HTTP_GET, handleAutomationsStoreGet);
    registerUri(server, "/api/automations/restore", HTTP_POST, handleAutomationsRestorePost);
    registerUri(server, "/api/scenes", HTTP_GET, handleScenesGet);
    registerUri(server, "/api/scenes/store", HTTP_GET, handleScenesStoreGet);
    registerUri(server, "/api/scenes/restore", HTTP_POST, handleScenesRestorePost);
    registerUri(server, "/api/cameras", HTTP_GET, handleCamerasGet);
    registerUri(server, "/api/cameras/store", HTTP_GET, handleCamerasStoreGet);
    registerUri(server, "/api/cameras/restore", HTTP_POST, handleCamerasRestorePost);
    registerUri(server, "/api/devices", HTTP_GET, handleDevicesGet);
    registerUri(server, "/api/devices/store", HTTP_GET, handleDevicesStoreGet);
    registerUri(server, "/api/history", HTTP_GET, handleHistoryGet);
    registerUri(server, "/api/settings/export", HTTP_GET, handleSettingsExportGet);
    registerUri(server, "/api/settings/restore", HTTP_POST, handleSettingsRestorePost);
}

const EmailSettings *hostProductEmailSettings(void) {
    if (s_settings == nullptr) {
        return nullptr;
    }
    return &s_settings->getSettings()->email;
}

void hostProductTick(void) {
    if (s_consoles != nullptr) {
        s_consoles->persistIfDue();
    }
    if (s_automations != nullptr) {
        s_automations->persistIfDue();
    }
    if (s_scenes != nullptr) {
        s_scenes->persistIfDue();
    }
    if (s_cameras != nullptr) {
        s_cameras->persistIfDue();
    }
    if (s_users != nullptr) {
        s_users->persistIfDue();
    }
    if (s_interconnect != nullptr) {
        s_interconnect->persistIfDue();
    }
    if (s_settings != nullptr) {
        // Device file persist is request-driven via saveDevicesJson.
    }
}

#endif
