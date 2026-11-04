#pragma once

#include "Defines.h"

#include <stdint.h>

class SettingsManager {
public:
    static bool isValidHistoryEntrySize(uint16_t entrySize);
    static uint8_t clampHistoryMaxStoreDays(uint8_t days);
    static uint8_t clampHistorySweepIntervalMin(uint8_t minutes);
};
