#pragma once

#include "Defines.h"

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

class DeviceTopicMap;
class SdCardStorage;

// Keep HistoryStore.h free of ReportableData/DeviceTopicMap bodies so native
// unit tests can compile against lightweight stubs.
constexpr size_t kHistoryValueTextMaxChars = 31;
constexpr size_t kHistoryValueTextMaxLen = kHistoryValueTextMaxChars + 1;

enum HistorySeriesKind : uint8_t {
    HistorySeriesMainState = 0,
    HistorySeriesReportable = 1
};

enum HistoryValueType : uint8_t {
    HistoryValueFloat = 0,
    HistoryValueInt = 1,
    HistoryValueBool = 2,
    HistoryValueString = 3
};

struct HistorySeriesKey {
    uint8_t ieee[8];
    HistorySeriesKind kind;
    uint8_t channel;
    uint16_t cluster;
    uint16_t attribute;
};

struct HistorySample {
    uint32_t epoch2010;
    HistoryValueType valueType;
    float floatValue;
    int32_t intValue;
    bool boolValue;
    char stringValue[kHistoryValueTextMaxLen];
};

struct HistoryStoreStats {
    uint16_t entriesCount;
    uint16_t pagesUsed;
    uint16_t pagesTotal;
    uint32_t valuesStored;
    uint32_t slabUsedKiB;
};

class HistoryStore {
public:
    HistoryStore();

    bool begin(uint16_t maxSizeKiB, uint16_t entrySizeBytes);
    void end();
    bool available() const { return slab != nullptr && pageCount > 0; }

    // Snapshot of filled entries + used pages (LittleFS or SD when usable).
    void setSdBackend(SdCardStorage *sdStorage);
    bool persistOnSd() const;
    void setSdBackupIntervalMin(uint16_t minutes);
    uint16_t sdBackupIntervalMin() const { return sdBackupIntervalMinutes; }
    // Seconds until next periodic SD backup; 0 when SD persist inactive.
    uint32_t sdSecondsToSave() const;

    bool persistSnapshot();
    bool restoreSnapshot();
    void discardSnapshotFile();
    bool snapshotFilePresent() const;

    void syncSeriesFromDevices(DeviceTopicMap *deviceMap);
    bool removeSeries(const HistorySeriesKey &key);
    void setRetention(uint8_t maxStoreDays, uint8_t sweepIntervalMin);
    void loop();

    bool noteFloat(
        const HistorySeriesKey &key,
        float value,
        float epsilon,
        HistoryValueType valueType
    );
    bool noteBool(const HistorySeriesKey &key, bool value);
    bool noteString(const HistorySeriesKey &key, const char *value);
    bool noteInt(const HistorySeriesKey &key, int32_t value, float epsilon);

    bool query(
        const HistorySeriesKey &key,
        uint32_t fromEpoch2010,
        uint32_t toEpoch2010,
        uint16_t maxPoints,
        HistorySample *outSamples,
        uint16_t *outCount,
        HistoryValueType *outType
    ) const;

    HistoryStoreStats stats() const;
    static uint32_t unixToEpoch2010(time_t unixSec);
    static time_t epoch2010ToUnix(uint32_t epoch2010);
    static bool clockIsSane(time_t unixSec);
    static uint16_t maxPagesForEntrySize(uint16_t entrySizeBytes);

#if defined(HISTORY_STORE_UNIT_TEST)
    static void setTestUnixNow(time_t unixSec);
    static void clearTestUnixNow();
    void forceSweepForTest();
    bool seriesRingState(
        const HistorySeriesKey &key,
        uint16_t *oldestIndex,
        uint16_t *newestIndex,
        uint16_t *pageCountOut
    ) const;
    bool exportSnapshotToHeap(uint8_t **outBytes, size_t *outLen) const;
    bool importSnapshotFromHeap(const uint8_t *bytes, size_t len);
#endif

private:
    struct PageHeader {
        uint16_t recordsCount;
        uint8_t blockLength;
        uint8_t reserved;
    };

    struct PageRecord {
        uint32_t startDate;
        uint32_t lastDate;
        uint32_t pageSlot;
    };

    struct EntryHeader {
        uint32_t startDate;
        uint32_t lastDate;
        uint16_t oldestIndex;
        uint16_t newestIndex;
    };

    struct SeriesRuntime {
        HistorySeriesKey key;
        HistoryValueType valueType;
        uint8_t blockLength;
        float epsilon;
        bool hasLast;
        float lastFloat;
        int32_t lastInt;
        bool lastBool;
        char lastString[kHistoryValueTextMaxLen];
        uint8_t *entryBytes;
        bool used;
    };

    uint8_t *slab = nullptr;
    uint8_t *freeBitmap = nullptr;
    uint16_t pageCount = 0;
    uint16_t entrySize = DEFAULT_HISTORY_ENTRY_SIZE;
    uint16_t maxPagesPerEntry = 0;
    uint8_t maxStoreDays = DEFAULT_HISTORY_MAX_STORE_DAYS;
    uint8_t sweepIntervalMin = DEFAULT_HISTORY_SWEEP_INTERVAL_MIN;
    unsigned long lastSweepMs = 0;
    uint32_t valuesStored = 0;
    SeriesRuntime *series = nullptr;
    uint16_t seriesCapacity = 0;
    uint16_t seriesCount = 0;
    SdCardStorage *sdBackend = nullptr;
    uint16_t sdBackupIntervalMinutes = DEFAULT_HISTORY_SD_BACKUP_INTERVAL_MIN;
    unsigned long lastSdBackupMs = 0;

    bool allocSlab(uint16_t maxSizeKiB);
    void freeSlab();
    int16_t acquirePage();
    void releasePage(uint16_t slot);
    uint8_t *pagePtr(uint16_t slot) const;
    uint32_t countValuesInPages() const;
    void refreshValuesStored();
    EntryHeader *entryHeader(SeriesRuntime *runtime) const;
    PageRecord *entryPageRecords(SeriesRuntime *runtime) const;
    uint16_t entryPageCount(const SeriesRuntime *runtime) const;
    SeriesRuntime *findSeries(const HistorySeriesKey &key) const;
    SeriesRuntime *ensureSeries(
        const HistorySeriesKey &key,
        HistoryValueType valueType,
        uint8_t blockLength,
        float epsilon
    );
    void dropSeries(SeriesRuntime *runtime);
    bool seriesConfigured(DeviceTopicMap *deviceMap, const HistorySeriesKey &key) const;
    bool appendRecord(SeriesRuntime *runtime, uint32_t epoch2010, const void *valueBytes);
    bool purgeOldestPage(SeriesRuntime *runtime);
    void purgeOutdated(SeriesRuntime *runtime, uint32_t nowEpoch2010);
    void sweepAll();
    bool keysEqual(const HistorySeriesKey &left, const HistorySeriesKey &right) const;
    uint32_t crc32Update(uint32_t crc, const uint8_t *data, size_t len) const;
    uint32_t crc32Finish(uint32_t crc) const;
    bool littleFsHasConfigSafeSpace(size_t extraBytes) const;
    bool writeSnapshotFile(const uint8_t *bytes, size_t len) const;
    bool readSnapshotFile(uint8_t **outBytes, size_t *outLen) const;
    bool buildSnapshot(uint8_t **outBytes, size_t *outLen) const;
    bool applySnapshot(const uint8_t *bytes, size_t len);
};

void historyNoteReportableValue(
    HistoryStore *store,
    DeviceTopicMap *deviceMap,
    const uint8_t ieee[8],
    uint16_t cluster,
    uint16_t attribute,
    const char *valueText
);
void historyNoteUserDataBraceList(
    HistoryStore *store,
    DeviceTopicMap *deviceMap,
    const uint8_t ieee[8],
    const char *braceList
);
void historyNoteMainStateValue(
    HistoryStore *store,
    DeviceTopicMap *deviceMap,
    const uint8_t ieee[8],
    uint8_t channel,
    const char *valueText
);
