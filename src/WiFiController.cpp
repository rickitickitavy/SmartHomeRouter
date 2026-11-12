#include "WiFiController.h"
#include "Logger.h"
#include "Defines.h"

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_coexist.h>
#include <esp_system.h>
#include <esp_log.h>
#include <nvs_flash.h>

static bool parseApIp(const char *text, IPAddress &address) {
    if (text == nullptr || text[0] == '\0') {
        return address.fromString(WIFI_DEFAULT_AP_IP);
    }
    if (!address.fromString(text)) {
        return address.fromString(WIFI_DEFAULT_AP_IP);
    }
    return true;
}

WiFiController::WiFiController(SettingsManager *settingsManager, bool forceAp)
    : settingsManager(settingsManager) {
    GlobalSettings *settings = settingsManager->getSettings();
    WiFi.persistent(false);
#if defined(BOARD_HOST_P4)
    // ESP-Hosted: keep WIFI_INIT_CONFIG_DEFAULT layout (incl. wifi_task_stack_size)
    // aligned with hybrid-built esp_hosted; avoid Arduino cache_tx_buf_num=4.
    WiFi.useStaticBuffers(true);
    esp_log_level_set("rpc_core", ESP_LOG_INFO);
    esp_log_level_set("rpc_rsp", ESP_LOG_INFO);
    esp_log_level_set("rpc_wrap", ESP_LOG_INFO);
    esp_log_level_set("H_API", ESP_LOG_INFO);
    esp_log_level_set("transport", ESP_LOG_INFO);
    esp_log_level_set("H_SDIO_DRV", ESP_LOG_INFO);
    // Hosted wifi_init path expects NVS ready even when nvs_enable is forced off.
    esp_err_t nvsErr = nvs_flash_init();
    if (nvsErr == ESP_ERR_NVS_NO_FREE_PAGES || nvsErr == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvsErr = nvs_flash_init();
    }
    if (nvsErr != ESP_OK) {
        LOGGER.warning(String("P4 NVS init 0x") + String((int)nvsErr, HEX));
    }
#endif
    WiFi.mode(WIFI_OFF);
    const bool coldStart = esp_reset_reason() == ESP_RST_POWERON
        || esp_reset_reason() == ESP_RST_BROWNOUT;
    delay(coldStart ? 150 : 800);
    WiFi.setHostname(settings->wifi.deviceName);

    if (forceAp || settings->wifi.mode == WifiSettingsModeAp || settings->wifi.bssid[0] == '\0') {
        startAp(false);
        return;
    }

    startSta();
    staBootJoinPending = true;
    staBootJoinStartedMs = millis();
}

bool WiFiController::isApMode() const {
    return apActive;
}

bool WiFiController::isStaConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

bool WiFiController::hasUsableInterface() const {
    if (isStaConnected()) {
        return true;
    }
    return apActive && isApRadioUp();
}

void WiFiController::setInterfaceReadyHandler(InterfaceReadyFn handler) {
    interfaceReadyHandler = handler;
    staWebRebindArmed = handler != nullptr;
    staNeedsWebRebind = false;
}

void WiFiController::requestStaWebRebind() {
    if (!staWebRebindArmed || apActive || !staEnabledAtBoot) {
        return;
    }
    staNeedsWebRebind = true;
}

void WiFiController::notifyStaDisconnected() {
    LOGGER.warning("STA event: disconnected or lost IP");
    requestStaWebRebind();
}

void WiFiController::notifyStaGotIp() {
    LOGGER.info("STA event: got IP " + WiFi.localIP().toString());
    requestStaWebRebind();
}

void WiFiController::runPendingStaWebRebind(bool staHasIpv4) {
    if (!staHasIpv4 || !staNeedsWebRebind || interfaceReadyHandler == nullptr) {
        return;
    }
    interfaceReadyHandler();
    staNeedsWebRebind = false;
}

String WiFiController::apNetworkName() const {
    GlobalSettings *settings = settingsManager->getSettings();
    String name = String(settings->wifi.bssid);
    if (name.length() == 0) {
        name = WIFI_DEFAULT_BSSID;
    }
    if (name.length() > 32) {
        name = name.substring(0, 32);
    }
    return name;
}

String WiFiController::apPassword() const {
    GlobalSettings *settings = settingsManager->getSettings();
    if (settings->wifi.password[0] == '\0') {
        return String(WIFI_DEFAULT_PASSWORD);
    }
    return String(settings->wifi.password);
}

bool WiFiController::isApRadioUp() const {
    const wifi_mode_t mode = WiFi.getMode();
    return mode == WIFI_AP || mode == WIFI_AP_STA;
}

void WiFiController::stopStationBeforeAp() {
    const wifi_mode_t currentMode = WiFi.getMode();
    if (currentMode != WIFI_STA && currentMode != WIFI_AP_STA) {
        return;
    }
    LOGGER.info("Stopping STA before AP");
    WiFi.disconnect(false, false, 2000);
    delay(200);
    WiFi.mode(WIFI_OFF);
    delay(400);
}

void WiFiController::startAp(bool useRecoveryIdentity) {
    recoveryApIdentity = useRecoveryIdentity;
    LOGGER.info(useRecoveryIdentity ? "Starting recovery WiFi AP..." : "Starting WiFi AP mode...");
    WiFi.persistent(false);
    WiFi.setSleep(false);
    stopStationBeforeAp();

    WiFi.mode(WIFI_AP);
    delay(200);
    esp_wifi_set_ps(WIFI_PS_NONE);
    const uint8_t apProtocol = WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N;
    if (esp_wifi_set_protocol(WIFI_IF_AP, apProtocol) != ESP_OK) {
        LOGGER.error("AP protocol 11b/g/n failed");
    }

    const String ssid = useRecoveryIdentity ? String(WIFI_DEFAULT_BSSID) : apNetworkName();
    const String password = useRecoveryIdentity ? String(WIFI_DEFAULT_PASSWORD) : apPassword();
    LOGGER.info(
        String("AP passphrase length ") + String(password.length())
            + (password == WIFI_DEFAULT_PASSWORD ? ", default" : ", custom")
    );

    IPAddress apIp;
    parseApIp(settingsManager->getSettings()->wifi.apIp, apIp);
    const IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIp, apIp, subnet);

    const bool ok = WiFi.softAP(ssid.c_str(), password.c_str(), 6, 0, 4);
    if (ok) {
        // Hosted SoftAP may still be asserting AP_START when create() returns; wait briefly.
        for (int i = 0; i < 50 && !WiFi.AP.started(); ++i) {
            delay(20);
        }
        WiFi.setTxPower(WIFI_POWER_8_5dBm);
    }

    apActive = ok;
    staEnabledAtBoot = false;
    lastApRestartMs = millis();
    if (ok) {
        const String apIpStr = WiFi.softAPIP().toString();
        const String apMacStr = WiFi.softAPmacAddress();
#if defined(BOARD_HOST_P4)
        ESP_LOGI("WiFiCtrl", "SoftAP up ssid=%s ip=%s mac=%s", ssid.c_str(), apIpStr.c_str(), apMacStr.c_str());
#endif
        LOGGER.info("AP " + ssid + " IP " + apIpStr + " MAC " + apMacStr);
        LOGGER.info("STA MAC " + WiFi.macAddress());
    } else {
        LOGGER.error("softAP FAILED");
    }
}

void WiFiController::applyStaRadio() {
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.setTxPower(WIFI_POWER_21dBm);
    esp_wifi_set_max_tx_power(84);
    // ESP32-P4 host builds lack on-chip coexist (Wi‑Fi is often ESP-Hosted); S3 keeps preference.
#if !defined(BOARD_HOST_P4)
    esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
#endif
}

void WiFiController::enableIeee154Coex() {
#if CONFIG_ESP_COEX_SW_COEXIST_ENABLE && CONFIG_SOC_IEEE802154_SUPPORTED
    if (esp_coex_wifi_i154_enable() != ESP_OK) {
        LOGGER.error("Wi-Fi/802.15.4 coexist enable failed");
        return;
    }
    LOGGER.info("Wi-Fi/802.15.4 coexist enabled");
#else
    LOGGER.warning("Wi-Fi/802.15.4 coexist API not in this build");
#endif
}

int WiFiController::staWifiChannel() const {
    return WiFi.channel();
}

uint8_t WiFiController::zigbeeChannelOverlappingSta() const {
    const int wifiChannel = WiFi.channel();
    if (wifiChannel < 1 || wifiChannel > 13) {
        return 0;
    }
    int zigbeeChannel = wifiChannel + 10;
    if (zigbeeChannel < 11) {
        zigbeeChannel = 11;
    }
    if (zigbeeChannel > 26) {
        zigbeeChannel = 26;
    }
    return (uint8_t)zigbeeChannel;
}

void WiFiController::onStaConnected() {
    applyStaRadio();
    LOGGER.info(
        "STA ready. IP " + WiFi.localIP().toString() + ", RSSI " + String(WiFi.RSSI())
            + " dBm MAC " + WiFi.macAddress()
    );
}

void WiFiController::beginStaJoin() {
    GlobalSettings *settings = settingsManager->getSettings();
    String targetSsid = String(settings->wifi.bssid);
    targetSsid.trim();

    wifi_country_t country;
    memset(&country, 0, sizeof(country));
    country.cc[0] = '0';
    country.cc[1] = '1';
    country.schan = 1;
    country.nchan = 13;
    country.max_tx_power = 20;
    country.policy = WIFI_COUNTRY_POLICY_MANUAL;
    esp_wifi_set_country(&country);
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

    LOGGER.info("STA scanning 2.4 GHz for '" + targetSsid + "'");
    const int16_t foundCount = WiFi.scanNetworks(false, true, false, 300);
    finishStaJoinFromScan(foundCount);
}

void WiFiController::beginStaJoinAsync() {
    if (staScanActive) {
        return;
    }
    GlobalSettings *settings = settingsManager->getSettings();
    String targetSsid = String(settings->wifi.bssid);
    targetSsid.trim();

    wifi_country_t country;
    memset(&country, 0, sizeof(country));
    country.cc[0] = '0';
    country.cc[1] = '1';
    country.schan = 1;
    country.nchan = 13;
    country.max_tx_power = 20;
    country.policy = WIFI_COUNTRY_POLICY_MANUAL;
    esp_wifi_set_country(&country);
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

    LOGGER.info("STA async scanning 2.4 GHz for '" + targetSsid + "'");
    const int16_t scanStarted = WiFi.scanNetworks(true, true, false, 300);
    if (scanStarted == WIFI_SCAN_FAILED) {
        LOGGER.warning("STA async scan failed to start; joining without scan");
        WiFi.begin(targetSsid.c_str(), settings->wifi.password);
        applyStaRadio();
        return;
    }
    staScanActive = true;
}

void WiFiController::finishStaJoinFromScan(int16_t foundCount) {
    GlobalSettings *settings = settingsManager->getSettings();
    String targetSsid = String(settings->wifi.bssid);
    targetSsid.trim();

    int matchIndex = -1;
    int32_t bestRssi = -127;
    if (foundCount <= 0) {
        LOGGER.warning("STA scan found no networks");
    }
    for (int16_t networkIndex = 0; networkIndex < foundCount; networkIndex++) {
        const String seenSsid = WiFi.SSID(networkIndex);
        const int32_t seenRssi = WiFi.RSSI(networkIndex);
        LOGGER.info(
            "STA saw " + seenSsid + " ch " + String(WiFi.channel(networkIndex))
                + " RSSI " + String(seenRssi)
        );
        if (seenSsid == targetSsid && seenRssi > bestRssi) {
            bestRssi = seenRssi;
            matchIndex = networkIndex;
        }
    }

    if (matchIndex >= 0) {
        LOGGER.info(
            "STA joining '" + targetSsid + "' on ch " + String(WiFi.channel(matchIndex))
                + " RSSI " + String(bestRssi)
        );
        WiFi.begin(
            targetSsid.c_str(),
            settings->wifi.password,
            WiFi.channel(matchIndex),
            WiFi.BSSID(matchIndex)
        );
    } else {
        LOGGER.warning("STA target not in 2.4 GHz scan (5 GHz-only or hidden); trying begin");
        WiFi.begin(targetSsid.c_str(), settings->wifi.password);
    }
    WiFi.scanDelete();
    applyStaRadio();
}

void WiFiController::reconnectSta() {
    GlobalSettings *settings = settingsManager->getSettings();
    LOGGER.warning("STA reconnecting to '" + String(settings->wifi.bssid) + "'...");
    WiFi.setSleep(false);
    WiFi.disconnect(false, false);
    delay(100);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    beginStaJoinAsync();
}

void WiFiController::startSta() {
    GlobalSettings *settings = settingsManager->getSettings();
    LOGGER.info(
        "Starting STA to '" + String(settings->wifi.bssid) + "' as " + String(settings->wifi.deviceName)
    );
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setHostname(settings->wifi.deviceName);
    delay(200);
    staEnabledAtBoot = true;
    beginStaJoinAsync();
    LOGGER.info("STA MAC " + WiFi.macAddress());
}

void WiFiController::update() {
    if (apActive) {
        if (!isApRadioUp() && (millis() - lastApRestartMs) >= kApRestartMs) {
            LOGGER.error("AP dropped; restarting");
            startAp(recoveryApIdentity);
        }
        return;
    }

    if (!staEnabledAtBoot) {
        return;
    }

    if (staScanActive) {
        const int16_t scanResult = WiFi.scanComplete();
        if (scanResult == WIFI_SCAN_RUNNING) {
            return;
        }
        staScanActive = false;
        if (scanResult == WIFI_SCAN_FAILED) {
            LOGGER.warning("STA async scan failed; joining without scan");
            GlobalSettings *settings = settingsManager->getSettings();
            WiFi.begin(settings->wifi.bssid, settings->wifi.password);
            applyStaRadio();
        } else {
            finishStaJoinFromScan(scanResult);
        }
    }

    if (staBootJoinPending) {
        if (isStaConnected()) {
            staWasConnected = true;
            onStaConnected();
            staBootJoinPending = false;
        } else if (!staScanActive && (millis() - staBootJoinStartedMs) >= kStaJoinTimeoutMs) {
            LOGGER.warning("STA join failed; starting recovery AP");
            staBootJoinPending = false;
            startAp(true);
            return;
        }
    }

    const bool connected = isStaConnected();
    if (!connected && staWasConnected) {
        LOGGER.warning("STA disconnected; reconnecting");
        requestStaWebRebind();
        lastStaReconnectMs = millis();
        reconnectSta();
    }
    if (connected && !staWasConnected) {
        onStaConnected();
    }
    runPendingStaWebRebind(connected);
    if (connected && (millis() - lastStaRadioRefreshMs) >= kStaRadioRefreshMs) {
        lastStaRadioRefreshMs = millis();
        applyStaRadio();
    }
    staWasConnected = connected;

    if (!connected && !staScanActive && (millis() - lastStaReconnectMs) >= kStaReconnectMs) {
        lastStaReconnectMs = millis();
        reconnectSta();
    }
}
