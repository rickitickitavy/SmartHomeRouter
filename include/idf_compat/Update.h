#pragma once

// Arduino Update API shim → ESP-IDF esp_ota_* for host/slave IDF builds.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"

#ifndef U_FLASH
#define U_FLASH 0
#endif

class UpdateClass {
public:
    bool begin(size_t size, int /*command*/ = U_FLASH) {
        abort();
        if (size == 0) {
            return false;
        }
        const esp_partition_t *update = esp_ota_get_next_update_partition(nullptr);
        if (update == nullptr) {
            return false;
        }
        esp_err_t err = esp_ota_begin(update, size, &handle_);
        if (err != ESP_OK) {
            handle_ = 0;
            return false;
        }
        partition_ = update;
        expected_ = size;
        written_ = 0;
        active_ = true;
        return true;
    }

    size_t write(const uint8_t *data, size_t length) {
        if (!active_ || handle_ == 0 || data == nullptr || length == 0) {
            return 0;
        }
        if (esp_ota_write(handle_, data, length) != ESP_OK) {
            return 0;
        }
        written_ += length;
        return length;
    }

    bool end(bool /*evenIfRemaining*/ = true) {
        if (!active_ || handle_ == 0 || partition_ == nullptr) {
            return false;
        }
        esp_err_t err = esp_ota_end(handle_);
        handle_ = 0;
        active_ = false;
        if (err != ESP_OK) {
            partition_ = nullptr;
            return false;
        }
        err = esp_ota_set_boot_partition(partition_);
        partition_ = nullptr;
        return err == ESP_OK;
    }

    void abort() {
        if (handle_ != 0) {
            esp_ota_abort(handle_);
            handle_ = 0;
        }
        partition_ = nullptr;
        active_ = false;
        expected_ = 0;
        written_ = 0;
    }

    bool isRunning() const {
        return active_;
    }

private:
    esp_ota_handle_t handle_ = 0;
    const esp_partition_t *partition_ = nullptr;
    size_t expected_ = 0;
    size_t written_ = 0;
    bool active_ = false;
};

static UpdateClass Update;
