#include "SdCardStorage.h"

#include "Defines.h"
#include "Logger.h"
#include "pins.h"

#if defined(BOARD_ROLE_HOST) && defined(BOARD_HOST_P4)
#include <SD_MMC.h>
#include <Arduino.h>
#endif

namespace {
constexpr const char *kSdMountPoint = "/sdcard";
}

const char *SdCardStorage::statusText() const {
    switch (cardStatus) {
        case SdCardStatusReady:
            return "ready";
        case SdCardStatusError:
            return "error";
        case SdCardStatusAbsent:
        default:
            return "absent";
    }
}

void SdCardStorage::setHistorySnapshotBytes(uint64_t bytes) {
    cachedHistorySnapshotKiB = (uint32_t)((bytes + 512ULL) / 1024ULL);
}

void SdCardStorage::clearHistorySnapshotSize() {
    cachedHistorySnapshotKiB = 0;
}

#if !(defined(BOARD_ROLE_HOST) && defined(BOARD_HOST_P4))

bool SdCardStorage::beginBootProbe() {
    cardUsable = false;
    cardStatus = SdCardStatusAbsent;
    cachedCapacityMb = 0;
    cachedFreeMb = 0;
    clearHistorySnapshotSize();
    mounted = false;
    return false;
}

bool SdCardStorage::remount() {
    return false;
}

void SdCardStorage::unmount() {
    mounted = false;
}

void SdCardStorage::refreshSizeCache() {
}

void SdCardStorage::ensurePowerOn() {
}

bool SdCardStorage::configurePins() {
    return false;
}

bool SdCardStorage::mountInternal(bool formatIfNeeded) {
    (void)formatIfNeeded;
    return false;
}

#else

void SdCardStorage::ensurePowerOn() {
    if (powerEnabled) {
        return;
    }
    pinMode(PIN_SD_POWER, OUTPUT);
    digitalWrite(PIN_SD_POWER, LOW);  // active-low enable
    delay(50);
    powerEnabled = true;
    LOGGER.info("SD power enabled (GPIO" + String((int)PIN_SD_POWER) + " LOW)");
}

bool SdCardStorage::configurePins() {
    if (pinsConfigured) {
        return true;
    }
    if (!SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0, PIN_SD_D1, PIN_SD_D2, PIN_SD_D3)) {
        LOGGER.error("SD_MMC.setPins failed");
        return false;
    }
    pinsConfigured = true;
    return true;
}

void SdCardStorage::refreshSizeCache() {
    if (!mounted) {
        return;
    }
    const uint64_t totalBytes = SD_MMC.totalBytes();
    const uint64_t usedBytes = SD_MMC.usedBytes();
    const uint64_t freeBytes = totalBytes > usedBytes ? (totalBytes - usedBytes) : 0;
    cachedCapacityMb = (uint32_t)((totalBytes + (1024ULL * 1024ULL / 2ULL)) / (1024ULL * 1024ULL));
    cachedFreeMb = (uint32_t)((freeBytes + (1024ULL * 1024ULL / 2ULL)) / (1024ULL * 1024ULL));
}

bool SdCardStorage::mountInternal(bool formatIfNeeded) {
    ensurePowerOn();
    if (!configurePins()) {
        cardStatus = SdCardStatusError;
        return false;
    }
    if (mounted) {
        return true;
    }
    // Keep -DDISABLE_SD for ESP Mail Client (mail must not claim SD). History uses SD_MMC directly.
    const bool ok = SD_MMC.begin(kSdMountPoint, false, formatIfNeeded, SDMMC_FREQ_DEFAULT);
    if (!ok) {
        LOGGER.warning(
            formatIfNeeded
                ? "SD mount failed after format attempt"
                : "SD mount failed (no card or unusable FS)"
        );
        return false;
    }
    mounted = true;
    refreshSizeCache();
    LOGGER.info(
        "SD mounted capacityMb="
        + String((unsigned long)cachedCapacityMb)
        + " freeMb="
        + String((unsigned long)cachedFreeMb)
    );
    return true;
}

void SdCardStorage::unmount() {
    if (!mounted) {
        return;
    }
    SD_MMC.end();
    mounted = false;
    LOGGER.info("SD unmounted (mount RAM released)");
}

bool SdCardStorage::remount() {
    if (mounted) {
        refreshSizeCache();
        return true;
    }
    if (!cardUsable && cardStatus == SdCardStatusAbsent) {
        return false;
    }
    LOGGER.info("SD remount for session");
    if (!mountInternal(false)) {
        cardStatus = SdCardStatusError;
        return false;
    }
    cardStatus = SdCardStatusReady;
    return true;
}

bool SdCardStorage::beginBootProbe() {
    cardUsable = false;
    cardStatus = SdCardStatusAbsent;
    cachedCapacityMb = 0;
    cachedFreeMb = 0;
    clearHistorySnapshotSize();
    LOGGER.info("SD boot probe start");
    ensurePowerOn();
    if (!configurePins()) {
        cardStatus = SdCardStatusError;
        return false;
    }

    if (mountInternal(false)) {
        cardUsable = true;
        cardStatus = SdCardStatusReady;
        LOGGER.info("SD boot probe ok (pre-formatted)");
        unmount();
        return true;
    }

    LOGGER.info("SD boot probe: first mount failed; trying format-if-needed");
    if (mountInternal(true)) {
        cardUsable = true;
        cardStatus = SdCardStatusReady;
        LOGGER.info("SD boot probe ok after format");
        unmount();
        return true;
    }

    // No card or hardware error — treat as absent for LittleFS fallback.
    cardUsable = false;
    cardStatus = SdCardStatusAbsent;
    cachedCapacityMb = 0;
    cachedFreeMb = 0;
    clearHistorySnapshotSize();
    LOGGER.info("SD absent or unusable; history stays on LittleFS");
    return false;
}

#endif
