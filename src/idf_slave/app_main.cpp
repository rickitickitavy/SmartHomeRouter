#include "Defines.h"

#include "esp_littlefs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>

static const char *kLogTag = "slave_idf";
static const char *kPartitionLabel = "spiffs";

// Arduino-shaped entry points live in main.cpp (BOARD_ROLE_SLAVE).
extern void setup(void);
extern void loop(void);

static bool slaveFsMount(void) {
    esp_vfs_littlefs_conf_t conf = {};
    conf.base_path = "/";
    conf.partition_label = kPartitionLabel;
    conf.format_if_mount_failed = true;
    conf.dont_mount = false;
    const esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(kLogTag, "LittleFS mount failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

extern "C" void app_main(void) {
    ESP_LOGI(kLogTag, "ESP-IDF slave bring-up %s", FIRMWARE_VERSION);
    if (!slaveFsMount()) {
        ESP_LOGE(kLogTag, "Continuing without LittleFS; stores may fail");
    }
    setup();
    while (true) {
        loop();
    }
}
