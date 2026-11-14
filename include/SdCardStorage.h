#pragma once

#include <stdint.h>

enum SdCardStatus : uint8_t {
    SdCardStatusAbsent = 0,
    SdCardStatusReady = 1,
    SdCardStatusError = 2
};

// P4 onboard MicroSD (SDMMC). Mount only for probe/restore/save sessions.
class SdCardStorage {
public:
    // Power on, mount or format-if-needed, cache capacity/free, then unmount.
    bool beginBootProbe();

    bool remount();
    void unmount();

    bool usable() const { return cardUsable; }
    bool isMounted() const { return mounted; }
    SdCardStatus status() const { return cardStatus; }
    uint32_t capacityMb() const { return cachedCapacityMb; }
    uint32_t freeMb() const { return cachedFreeMb; }
    // Real on-card history snapshot size from last I/O mount (0 if none).
    uint32_t historySnapshotKiB() const { return cachedHistorySnapshotKiB; }
    void setHistorySnapshotBytes(uint64_t bytes);
    void clearHistorySnapshotSize();

    // Refresh capacity/free while mounted; no-op if unmounted.
    void refreshSizeCache();

    const char *statusText() const;

private:
    bool powerEnabled = false;
    bool pinsConfigured = false;
    bool mounted = false;
    bool cardUsable = false;
    SdCardStatus cardStatus = SdCardStatusAbsent;
    uint32_t cachedCapacityMb = 0;
    uint32_t cachedFreeMb = 0;
    uint32_t cachedHistorySnapshotKiB = 0;

    void ensurePowerOn();
    bool configurePins();
    bool mountInternal(bool formatIfNeeded);
};
