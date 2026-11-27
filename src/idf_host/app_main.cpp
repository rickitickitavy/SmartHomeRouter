#include "Defines.h"
#include "HostFs.h"
#include "HostHttpServer.h"
#include "HostProductApi.h"
#include "HostServices.h"
#include "HostSlaveReset.h"
#include "HostWifi.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

static const char *kLogTag = "host_idf";
static const char *kStaCredPath = "/wifi_sta.txt";

static void onConsoleRebind(void) {
    hostHttpRebind();
}

static bool loadStaCredentials(char *ssidOut, size_t ssidLen, char *passOut, size_t passLen) {
    FILE *file = fopen(kStaCredPath, "r");
    if (file == nullptr) {
        return false;
    }
    if (fgets(ssidOut, static_cast<int>(ssidLen), file) == nullptr) {
        fclose(file);
        return false;
    }
    if (fgets(passOut, static_cast<int>(passLen), file) == nullptr) {
        passOut[0] = '\0';
    }
    fclose(file);
    ssidOut[strcspn(ssidOut, "\r\n")] = '\0';
    passOut[strcspn(passOut, "\r\n")] = '\0';
    return ssidOut[0] != '\0';
}

static void hostServiceTask(void * /*argument*/) {
    ESP_LOGI(kLogTag, "Host service task running");
    while (true) {
#if defined(BOARD_HOST_S3)
        hostWifiUpdateS3();
#elif defined(BOARD_HOST_P4)
        hostWifiUpdateP4();
#endif
        hostServicesUpdate();
        hostProductTick();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(kLogTag, "ESP-IDF host bring-up %s", FIRMWARE_VERSION);

    hostResetSlaveSynchronous();

    if (!hostFsMount() || !hostFsProbeRoundTrip()) {
        ESP_LOGE(kLogTag, "LittleFS bring-up failed");
    }

    // Seed SMTP configured-gate before services begin loads /email.txt.
    FILE *emailCfg = fopen("/email.txt", "w");
    if (emailCfg != nullptr) {
        fputs("smtp.example.local\n", emailCfg);
        fclose(emailCfg);
    }

    hostWifiSetRebindHandler(onConsoleRebind);

    char staSsid[33] = {};
    char staPassword[65] = {};
    const bool haveSta = loadStaCredentials(staSsid, sizeof(staSsid), staPassword, sizeof(staPassword));
#if defined(BOARD_HOST_S3)
    if (haveSta) {
        ESP_LOGI(kLogTag, "Found %s; starting STA join", kStaCredPath);
        hostWifiBeginS3(false, staSsid, staPassword, WIFI_DEFAULT_DEVICE_NAME);
    } else {
        ESP_LOGI(kLogTag, "No %s; starting SoftAP %s", kStaCredPath, WIFI_DEFAULT_BSSID);
        hostWifiBeginS3(true, nullptr, nullptr, WIFI_DEFAULT_DEVICE_NAME);
    }
#elif defined(BOARD_HOST_P4)
    if (haveSta) {
        ESP_LOGI(kLogTag, "Found %s; starting P4 Hosted STA join", kStaCredPath);
        hostWifiBeginP4(false, staSsid, staPassword, WIFI_DEFAULT_DEVICE_NAME);
    } else {
        ESP_LOGI(kLogTag, "No %s; starting P4 Hosted SoftAP %s", kStaCredPath, WIFI_DEFAULT_BSSID);
        hostWifiBeginP4(true, nullptr, nullptr, WIFI_DEFAULT_DEVICE_NAME);
    }
#endif

    if (!hostProductBegin()) {
        ESP_LOGE(kLogTag, "Product stores failed to start");
    }

    if (!hostHttpBegin()) {
        ESP_LOGE(kLogTag, "HTTP console failed to start");
    }

    hostServicesBegin();
    if (hostSmtpIsConfigured()) {
        hostSmtpEnqueue("idf-bringup", "SMTP queue smoke");
    }
    ESP_LOGI(
        kLogTag,
        "OTA hosted-c6 needed(unreadable)=%d needed(match)=%d spiOtaActive=%d",
        (int)hostOtaHostedC6Needed("2.12.11", nullptr, false),
        (int)hostOtaHostedC6Needed("2.12.11", "2.12.11", true),
        (int)hostSpiOtaIsActive()
    );
    hostInterconnectStartPairingWait();

    const BaseType_t created = xTaskCreate(
        hostServiceTask,
        "hostSvc",
        8192,
        nullptr,
        5,
        nullptr
    );
    if (created != pdPASS) {
        ESP_LOGE(kLogTag, "Failed to create host service task");
    }
}
