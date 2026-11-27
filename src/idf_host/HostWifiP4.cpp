#include "HostWifi.h"

#if defined(BOARD_ROLE_HOST) && defined(BOARD_HOST_P4)

#include "Defines.h"

#include "esp_event.h"
#include "esp_hosted.h"
#include "esp_hosted_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/ip4_addr.h"
#include "nvs_flash.h"

#include <string.h>

static const char *kLogTag = "host_wifi_p4";
static constexpr uint32_t kStaJoinTimeoutMs = 20000;
static constexpr uint32_t kStaReconnectMs = 10000;
static constexpr uint32_t kApRestartMs = 10000;
static constexpr int kSoftApChannel = 6;
static constexpr int kSoftApMaxConn = 4;

static esp_netif_t *s_apNetif = nullptr;
static esp_netif_t *s_staNetif = nullptr;
static bool s_wifiInited = false;
static bool s_apActive = false;
static bool s_recoveryApIdentity = false;
static bool s_staEnabledAtBoot = false;
static bool s_staWasConnected = false;
static bool s_staBootJoinPending = false;
static bool s_staNeedsWebRebind = false;
static bool s_staWebRebindArmed = false;
static bool s_staGotIp = false;
static uint32_t s_staBootJoinStartedMs = 0;
static uint32_t s_lastStaReconnectMs = 0;
static uint32_t s_lastApRestartMs = 0;
static char s_staSsid[33] = {};
static char s_staPassword[65] = {};
static HostWifiRebindFn s_rebindHandler = nullptr;

static uint32_t nowMs(void) {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

void hostWifiSetRebindHandler(HostWifiRebindFn handler) {
    s_rebindHandler = handler;
    s_staWebRebindArmed = handler != nullptr;
    s_staNeedsWebRebind = false;
}

static void requestStaWebRebind(const char *reason) {
    if (!s_staWebRebindArmed || s_apActive || !s_staEnabledAtBoot) {
        return;
    }
    if (!s_staNeedsWebRebind) {
        ESP_LOGI(kLogTag, "Web rebind armed: %s", reason != nullptr ? reason : "unknown");
    }
    s_staNeedsWebRebind = true;
}

static void runPendingStaWebRebind(bool staHasIpv4) {
    if (!staHasIpv4 || !s_staNeedsWebRebind || s_rebindHandler == nullptr) {
        return;
    }
    ESP_LOGI(kLogTag, "Running pending STA web rebind (service loop)");
    s_rebindHandler();
    s_staNeedsWebRebind = false;
}

static void notifyHostedTransportRecovered(const char *reason) {
    requestStaWebRebind(reason != nullptr ? reason : "Hosted transport recovery");
}

static void onEspHostedEvent(
    void * /*arg*/,
    esp_event_base_t /*eventBase*/,
    int32_t eventId,
    void * /*eventData*/
) {
    if (eventId == ESP_HOSTED_EVENT_TRANSPORT_UP) {
        notifyHostedTransportRecovered("Hosted TRANSPORT_UP");
        return;
    }
    if (eventId == ESP_HOSTED_EVENT_CP_INIT) {
        notifyHostedTransportRecovered("Hosted CP_INIT");
    }
}

static void ensureNvs(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static void applyStaRadio(void) {
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    esp_wifi_set_max_tx_power(84);
}

static void stopStationBeforeAp(void) {
    wifi_mode_t mode = WIFI_MODE_NULL;
    if (esp_wifi_get_mode(&mode) != ESP_OK) {
        return;
    }
    if (mode != WIFI_MODE_STA && mode != WIFI_MODE_APSTA) {
        return;
    }
    ESP_LOGI(kLogTag, "Stopping STA before AP");
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(200));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    vTaskDelay(pdMS_TO_TICKS(400));
}

static bool startAp(bool useRecoveryIdentity) {
    s_recoveryApIdentity = useRecoveryIdentity;
    ESP_LOGI(
        kLogTag,
        "%s",
        useRecoveryIdentity ? "Starting recovery WiFi AP..." : "Starting WiFi AP mode..."
    );

    stopStationBeforeAp();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    vTaskDelay(pdMS_TO_TICKS(200));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    const uint8_t apProtocol = WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N;
    if (esp_wifi_set_protocol(WIFI_IF_AP, apProtocol) != ESP_OK) {
        ESP_LOGE(kLogTag, "AP protocol 11b/g/n failed");
    }

    const char *ssid = WIFI_DEFAULT_BSSID;
    const char *password = WIFI_DEFAULT_PASSWORD;
    (void)useRecoveryIdentity;

    wifi_config_t apConfig = {};
    strncpy(reinterpret_cast<char *>(apConfig.ap.ssid), ssid, sizeof(apConfig.ap.ssid) - 1);
    strncpy(
        reinterpret_cast<char *>(apConfig.ap.password),
        password,
        sizeof(apConfig.ap.password) - 1
    );
    apConfig.ap.ssid_len = static_cast<uint8_t>(strlen(ssid));
    apConfig.ap.channel = kSoftApChannel;
    apConfig.ap.max_connection = kSoftApMaxConn;
    apConfig.ap.authmode = WIFI_AUTH_WPA2_PSK;

    esp_netif_ip_info_t ipInfo = {};
    IP4_ADDR(&ipInfo.ip, 192, 168, 0, 1);
    IP4_ADDR(&ipInfo.gw, 192, 168, 0, 1);
    IP4_ADDR(&ipInfo.netmask, 255, 255, 255, 0);
    ESP_ERROR_CHECK(esp_netif_dhcps_stop(s_apNetif));
    ESP_ERROR_CHECK(esp_netif_set_ip_info(s_apNetif, &ipInfo));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(s_apNetif));

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &apConfig));
    const esp_err_t startErr = esp_wifi_start();
    if (startErr != ESP_OK && startErr != ESP_ERR_WIFI_CONN) {
        ESP_LOGE(kLogTag, "esp_wifi_start AP failed: %s", esp_err_to_name(startErr));
        s_apActive = false;
        return false;
    }

    // Wait briefly: Hosted SoftAP may still be asserting AP_START.
    for (int i = 0; i < 50; ++i) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    esp_wifi_set_max_tx_power(34);

    s_apActive = true;
    s_staEnabledAtBoot = false;
    s_staGotIp = false;
    s_lastApRestartMs = nowMs();
    ESP_LOGI(kLogTag, "AP %s IP %s (ESP-Hosted)", ssid, WIFI_DEFAULT_AP_IP);
    return true;
}

static void beginStaJoin(void) {
    wifi_config_t staConfig = {};
    strncpy(reinterpret_cast<char *>(staConfig.sta.ssid), s_staSsid, sizeof(staConfig.sta.ssid) - 1);
    strncpy(
        reinterpret_cast<char *>(staConfig.sta.password),
        s_staPassword,
        sizeof(staConfig.sta.password) - 1
    );
    staConfig.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &staConfig));
    applyStaRadio();
    const esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGW(kLogTag, "esp_wifi_connect: %s", esp_err_to_name(err));
    }
}

static void startSta(void) {
    ESP_LOGI(kLogTag, "Starting STA to '%s' (ESP-Hosted)", s_staSsid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    vTaskDelay(pdMS_TO_TICKS(200));
    s_staEnabledAtBoot = true;
    s_apActive = false;
    beginStaJoin();
}

static void onWifiEvent(void * /*arg*/, esp_event_base_t eventBase, int32_t eventId, void *eventData) {
    if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_START) {
        if (s_staEnabledAtBoot && !s_apActive) {
            esp_wifi_connect();
        }
        return;
    }
    if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *disconnected =
            static_cast<wifi_event_sta_disconnected_t *>(eventData);
        ESP_LOGW(
            kLogTag,
            "STA disconnected reason=%u",
            disconnected != nullptr ? disconnected->reason : 0
        );
        s_staGotIp = false;
        requestStaWebRebind("STA disconnect/lost IP");
        return;
    }
    if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = static_cast<ip_event_got_ip_t *>(eventData);
        ESP_LOGI(kLogTag, "STA got IP " IPSTR, IP2STR(&event->ip_info.ip));
        s_staGotIp = true;
        s_staWasConnected = true;
        s_staBootJoinPending = false;
        requestStaWebRebind("STA got IP");
    }
}

static void registerHostedTransportEvents(void) {
    const esp_err_t err = esp_event_handler_register(
        ESP_HOSTED_EVENT,
        ESP_EVENT_ANY_ID,
        &onEspHostedEvent,
        nullptr
    );
    if (err != ESP_OK) {
        ESP_LOGW(kLogTag, "Hosted event register failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(kLogTag, "Hosted transport events armed for web console rebind");
}

static void initWifiStack(const char *hostname) {
    if (s_wifiInited) {
        return;
    }
    ensureNvs();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Bring up Hosted transport before remote Wi-Fi APIs.
    const int hostedInit = esp_hosted_init();
    if (hostedInit != 0) {
        ESP_LOGE(kLogTag, "esp_hosted_init failed: %d", hostedInit);
    } else {
        const int hostedConnect = esp_hosted_connect_to_slave();
        if (hostedConnect != 0) {
            ESP_LOGW(kLogTag, "esp_hosted_connect_to_slave: %d (continuing)", hostedConnect);
        }
    }

    s_apNetif = esp_netif_create_default_wifi_ap();
    s_staNetif = esp_netif_create_default_wifi_sta();
    if (hostname != nullptr && hostname[0] != '\0' && s_staNetif != nullptr) {
        esp_netif_set_hostname(s_staNetif, hostname);
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &onWifiEvent, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &onWifiEvent, nullptr));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    registerHostedTransportEvents();
    s_wifiInited = true;
}

void hostWifiBeginP4(
    bool forceAp,
    const char *staSsid,
    const char *staPassword,
    const char *hostname
) {
    initWifiStack(hostname != nullptr ? hostname : WIFI_DEFAULT_DEVICE_NAME);

    const esp_reset_reason_t resetReason = esp_reset_reason();
    const bool coldStart =
        resetReason == ESP_RST_POWERON || resetReason == ESP_RST_BROWNOUT;
    vTaskDelay(pdMS_TO_TICKS(coldStart ? 150 : 800));

    const bool haveStaSsid = staSsid != nullptr && staSsid[0] != '\0';
    if (forceAp || !haveStaSsid) {
        startAp(false);
        return;
    }

    strncpy(s_staSsid, staSsid, sizeof(s_staSsid) - 1);
    if (staPassword != nullptr) {
        strncpy(s_staPassword, staPassword, sizeof(s_staPassword) - 1);
    } else {
        s_staPassword[0] = '\0';
    }
    startSta();
    s_staBootJoinPending = true;
    s_staBootJoinStartedMs = nowMs();
}

bool hostWifiIsApModeP4(void) {
    return s_apActive;
}

bool hostWifiIsStaConnectedP4(void) {
    return s_staGotIp;
}

bool hostWifiHasUsableInterfaceP4(void) {
    if (hostWifiIsStaConnectedP4()) {
        return true;
    }
    return s_apActive;
}

void hostWifiUpdateP4(void) {
    if (s_apActive) {
        wifi_mode_t mode = WIFI_MODE_NULL;
        esp_wifi_get_mode(&mode);
        const bool apUp = mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA;
        if (!apUp && (nowMs() - s_lastApRestartMs) >= kApRestartMs) {
            ESP_LOGE(kLogTag, "AP dropped; restarting");
            startAp(s_recoveryApIdentity);
        }
        return;
    }

    if (!s_staEnabledAtBoot) {
        return;
    }

    if (s_staBootJoinPending) {
        if (hostWifiIsStaConnectedP4()) {
            s_staWasConnected = true;
            s_staBootJoinPending = false;
        } else if ((nowMs() - s_staBootJoinStartedMs) >= kStaJoinTimeoutMs) {
            ESP_LOGW(kLogTag, "STA join failed; starting recovery AP");
            s_staBootJoinPending = false;
            startAp(true);
            return;
        }
    }

    const bool connected = hostWifiIsStaConnectedP4();
    if (!connected && s_staWasConnected) {
        ESP_LOGW(kLogTag, "STA disconnected; reconnecting");
        requestStaWebRebind("polled STA disconnect");
        s_lastStaReconnectMs = nowMs();
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(100));
        beginStaJoin();
    }
    // Hosted recovery may arm rebind without classic GOT_IP; run when STA has IP.
    runPendingStaWebRebind(connected);
    s_staWasConnected = connected;

    if (!connected && (nowMs() - s_lastStaReconnectMs) >= kStaReconnectMs) {
        s_lastStaReconnectMs = nowMs();
        beginStaJoin();
    }
}

#endif
