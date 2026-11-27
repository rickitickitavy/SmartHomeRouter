#include "HostFs.h"

#if defined(BOARD_ROLE_HOST)

#include "esp_log.h"
#include "esp_littlefs.h"

#include <stdio.h>
#include <string.h>

static const char *kLogTag = "host_fs";
static const char *kPartitionLabel = "spiffs";
static const char *kProbePath = "/idf_fs_probe.txt";
static const char *kProbePayload = "idf-fs-ok";

bool hostFsMount(void) {
    esp_vfs_littlefs_conf_t conf = {};
    conf.base_path = "/";
    conf.partition_label = kPartitionLabel;
    conf.format_if_mount_failed = true;
    conf.dont_mount = false;

    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(kLogTag, "LittleFS mount failed: %s", esp_err_to_name(err));
        return false;
    }

    size_t totalBytes = 0;
    size_t usedBytes = 0;
    err = esp_littlefs_info(kPartitionLabel, &totalBytes, &usedBytes);
    if (err == ESP_OK) {
        ESP_LOGI(
            kLogTag,
            "LittleFS mounted label=%s used=%u total=%u",
            kPartitionLabel,
            (unsigned)usedBytes,
            (unsigned)totalBytes
        );
    } else {
        ESP_LOGI(kLogTag, "LittleFS mounted label=%s", kPartitionLabel);
    }
    return true;
}

bool hostFsProbeRoundTrip(void) {
    FILE *outFile = fopen(kProbePath, "w");
    if (outFile == nullptr) {
        ESP_LOGE(kLogTag, "Probe write open failed for %s", kProbePath);
        return false;
    }
    const int written = fputs(kProbePayload, outFile);
    fclose(outFile);
    if (written < 0) {
        ESP_LOGE(kLogTag, "Probe write failed for %s", kProbePath);
        return false;
    }

    FILE *inFile = fopen(kProbePath, "r");
    if (inFile == nullptr) {
        ESP_LOGE(kLogTag, "Probe read open failed for %s", kProbePath);
        return false;
    }
    char buffer[32];
    const char *line = fgets(buffer, sizeof(buffer), inFile);
    fclose(inFile);
    if (line == nullptr || strcmp(buffer, kProbePayload) != 0) {
        ESP_LOGE(kLogTag, "Probe round-trip mismatch for %s", kProbePath);
        return false;
    }

    ESP_LOGI(kLogTag, "Store-root probe round-trip ok (%s)", kProbePath);
    return true;
}

#endif
