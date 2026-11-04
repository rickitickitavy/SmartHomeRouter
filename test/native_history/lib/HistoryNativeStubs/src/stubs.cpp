#include "Defines.h"
#include "SettingsManager.h"
#include "esp_heap_caps.h"

#include <stdlib.h>
#include <string.h>

unsigned long millis() {
    return 0;
}

void *heap_caps_malloc(size_t size, uint32_t) {
    return malloc(size);
}

void *heap_caps_calloc(size_t n, size_t size, uint32_t) {
    return calloc(n, size);
}

void heap_caps_free(void *ptr) {
    free(ptr);
}

bool SettingsManager::isValidHistoryEntrySize(uint16_t entrySize) {
    return entrySize == 256 || entrySize == 512 || entrySize == 1024 || entrySize == 2048
        || entrySize == 4096;
}

uint8_t SettingsManager::clampHistoryMaxStoreDays(uint8_t days) {
    if (days < HISTORY_MAX_STORE_DAYS_MIN) {
        return HISTORY_MAX_STORE_DAYS_MIN;
    }
    if (days > HISTORY_MAX_STORE_DAYS_MAX) {
        return HISTORY_MAX_STORE_DAYS_MAX;
    }
    return days;
}

uint8_t SettingsManager::clampHistorySweepIntervalMin(uint8_t minutes) {
    if (minutes < HISTORY_SWEEP_INTERVAL_MIN_MIN) {
        return HISTORY_SWEEP_INTERVAL_MIN_MIN;
    }
    if (minutes > HISTORY_SWEEP_INTERVAL_MIN_MAX) {
        return HISTORY_SWEEP_INTERVAL_MIN_MAX;
    }
    return minutes;
}
