#include "HistoryStore.h"

#if defined(HISTORY_STORE_UNIT_TEST)
#include <esp_heap_caps.h>
// Match ReportableData.h without pulling host Arduino headers.
constexpr size_t kReportableValueTextMaxChars = kHistoryValueTextMaxChars;
#else
#include "Logger.h"
#include "SdCardStorage.h"
#include "SettingsManager.h"
#include "UserDataPayload.h"
#include "ZigbeeDeviceType.h"
#include <FS.h>
#include <LittleFS.h>
#if defined(BOARD_HOST_P4)
#include <SD_MMC.h>
#endif
#include <esp_heap_caps.h>
#endif

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace {

constexpr size_t kSeriesCapacityMax = 256;
constexpr time_t kSaneUnixFloor = 1700000000;
constexpr uint32_t kSnapshotMagic = 0x53485A48u; // 'HZHS' LE
constexpr uint16_t kSnapshotFormatVersion = 1;
constexpr size_t kSnapshotConfigHeadroomBytes = 96u * 1024u;
constexpr const char *kSnapshotPath = "/history/snapshot.bin";
constexpr const char *kSnapshotTempPath = "/history/snapshot.tmp";
constexpr const char *kSnapshotDir = "/history";
// SD_MMC FS paths are relative to the mount root (/sdcard).

#pragma pack(push, 1)
struct SnapshotFileHeader {
    uint32_t magic;
    uint16_t formatVersion;
    uint16_t entrySize;
    uint16_t pageBytes;
    uint16_t seriesCount;
    uint16_t packedPageCount;
    uint16_t reserved0;
    uint32_t valuesStored;
    uint32_t payloadCrc;
};

struct SnapshotSeriesHeader {
    uint8_t ieee[8];
    uint8_t kind;
    uint8_t channel;
    uint16_t cluster;
    uint16_t attribute;
    uint8_t valueType;
    uint8_t blockLength;
    uint8_t hasLast;
    uint8_t reserved0;
    float epsilon;
    float lastFloat;
    int32_t lastInt;
    uint8_t lastBool;
    uint8_t reserved1[3];
    char lastString[kHistoryValueTextMaxLen];
};
#pragma pack(pop)

#if defined(HISTORY_STORE_UNIT_TEST)
bool gHistoryTestUnixActive = false;
time_t gHistoryTestUnix = 0;

bool historyIsValidEntrySize(uint16_t entrySize) {
    return entrySize == 256 || entrySize == 512 || entrySize == 1024 || entrySize == 2048
        || entrySize == 4096;
}

uint8_t historyClampMaxStoreDays(uint8_t days) {
    if (days < HISTORY_MAX_STORE_DAYS_MIN) {
        return HISTORY_MAX_STORE_DAYS_MIN;
    }
    if (days > HISTORY_MAX_STORE_DAYS_MAX) {
        return HISTORY_MAX_STORE_DAYS_MAX;
    }
    return days;
}

uint8_t historyClampSweepIntervalMin(uint8_t minutes) {
    if (minutes < HISTORY_SWEEP_INTERVAL_MIN_MIN) {
        return HISTORY_SWEEP_INTERVAL_MIN_MIN;
    }
    if (minutes > HISTORY_SWEEP_INTERVAL_MIN_MAX) {
        return HISTORY_SWEEP_INTERVAL_MIN_MAX;
    }
    return minutes;
}
#else
bool historyIsValidEntrySize(uint16_t entrySize) {
    return SettingsManager::isValidHistoryEntrySize(entrySize);
}

uint8_t historyClampMaxStoreDays(uint8_t days) {
    return SettingsManager::clampHistoryMaxStoreDays(days);
}

uint8_t historyClampSweepIntervalMin(uint8_t minutes) {
    return SettingsManager::clampHistorySweepIntervalMin(minutes);
}
#endif

time_t historyWallClockUnix() {
#if defined(HISTORY_STORE_UNIT_TEST)
    if (gHistoryTestUnixActive) {
        return gHistoryTestUnix;
    }
#endif
    return time(nullptr);
}

bool floatChangedBeyond(float previous, float next, float epsilon) {
    if (epsilon <= 0.0f) {
        return previous != next;
    }
    return fabsf(next - previous) >= epsilon;
}

} // namespace

HistoryStore::HistoryStore() = default;

#if defined(HISTORY_STORE_UNIT_TEST)
void HistoryStore::setTestUnixNow(time_t unixSec) {
    gHistoryTestUnixActive = true;
    gHistoryTestUnix = unixSec;
}

void HistoryStore::clearTestUnixNow() {
    gHistoryTestUnixActive = false;
    gHistoryTestUnix = 0;
}

void HistoryStore::forceSweepForTest() {
    sweepAll();
}

bool HistoryStore::seriesRingState(
    const HistorySeriesKey &key,
    uint16_t *oldestIndex,
    uint16_t *newestIndex,
    uint16_t *pageCountOut
) const {
    if (oldestIndex == nullptr || newestIndex == nullptr || pageCountOut == nullptr) {
        return false;
    }
    SeriesRuntime *runtime = findSeries(key);
    if (runtime == nullptr || !runtime->used) {
        return false;
    }
    EntryHeader *header = entryHeader(runtime);
    if (header == nullptr) {
        return false;
    }
    *oldestIndex = header->oldestIndex;
    *newestIndex = header->newestIndex;
    *pageCountOut = entryPageCount(runtime);
    return true;
}
#endif

#if !defined(HISTORY_STORE_UNIT_TEST)
void historyNoteReportableValue(
    HistoryStore *store,
    DeviceTopicMap *deviceMap,
    const uint8_t ieee[8],
    uint16_t cluster,
    uint16_t attribute,
    const char *valueText
) {
    if (store == nullptr || deviceMap == nullptr || ieee == nullptr || valueText == nullptr
        || !store->available()) {
        return;
    }
    DeviceTopicEntry *entry = deviceMap->findByIeee(ieee);
    if (entry == nullptr || !entry->used || entry->reportableData == nullptr) {
        return;
    }
    const int rowIndex =
        reportableDataFindIndex(entry->reportableData, entry->reportableDataCount, cluster, attribute);
    if (rowIndex < 0) {
        return;
    }
    const ReportableDataDef &row = entry->reportableData[rowIndex];
    if (!reportableHistoryEnabled(&row)) {
        return;
    }
    HistorySeriesKey key{};
    memcpy(key.ieee, ieee, 8);
    key.kind = HistorySeriesReportable;
    key.cluster = cluster;
    key.attribute = attribute;
    if (row.type == ReportableDataTypeFloat) {
        store->noteFloat(key, strtof(valueText, nullptr), row.epsilon, HistoryValueFloat);
    } else if (row.type == ReportableDataTypeInt) {
        store->noteInt(key, (int32_t)strtol(valueText, nullptr, 10), row.epsilon);
    } else if (row.type == ReportableDataTypeBool) {
        const bool asBool =
            strcasecmp(valueText, "true") == 0 || strcasecmp(valueText, "on") == 0
            || strcmp(valueText, "1") == 0;
        store->noteBool(key, asBool);
    } else {
        store->noteString(key, valueText);
    }
}

void historyNoteUserDataBraceList(
    HistoryStore *store,
    DeviceTopicMap *deviceMap,
    const uint8_t ieee[8],
    const char *braceList
) {
    if (store == nullptr || deviceMap == nullptr || ieee == nullptr || braceList == nullptr
        || !store->available()) {
        return;
    }
    DeviceTopicEntry *entry = deviceMap->findByIeee(ieee);
    if (entry == nullptr || !entry->used || entry->userDataFields == nullptr
        || entry->userDataFieldCount == 0) {
        return;
    }
    String slots[kUserDataFieldMaxCount];
    const int slotCount =
        userDataSplitBraceSlots(String(braceList), slots, kUserDataFieldMaxCount);
    if (slotCount <= 0) {
        return;
    }
    const uint8_t fieldCount = entry->userDataFieldCount;
    for (uint8_t fieldIndex = 0; fieldIndex < fieldCount; fieldIndex++) {
        const UserDataFieldDef &field = entry->userDataFields[fieldIndex];
        if (!userDataHistoryEnabled(&field)) {
            continue;
        }
        if (fieldIndex >= (uint8_t)slotCount) {
            continue;
        }
        if (userDataSlotIsNullToken(slots[fieldIndex])) {
            continue;
        }
        String valueText = userDataUnquoteSlot(slots[fieldIndex]);
        HistorySeriesKey key{};
        memcpy(key.ieee, ieee, 8);
        key.kind = HistorySeriesReportable;
        key.cluster = kUserDataClusterId;
        key.attribute = userDataHistoryAttributeForFieldIndex(fieldIndex);
        const UserDataFieldType fieldType = (UserDataFieldType)field.type;
        if (fieldType == UserDataFieldTypeFloat) {
            store->noteFloat(key, strtof(valueText.c_str(), nullptr), field.epsilon, HistoryValueFloat);
        } else if (fieldType == UserDataFieldTypeInt) {
            store->noteInt(key, (int32_t)strtol(valueText.c_str(), nullptr, 10), field.epsilon);
        } else if (fieldType == UserDataFieldTypeBool) {
            bool asBool = false;
            if (!userDataParseBoolToken(valueText, &asBool)) {
                asBool =
                    strcasecmp(valueText.c_str(), "true") == 0
                    || strcasecmp(valueText.c_str(), "on") == 0
                    || strcmp(valueText.c_str(), "1") == 0;
            }
            store->noteBool(key, asBool);
        } else {
            userDataTruncateStringValue(&valueText);
            store->noteString(key, valueText.c_str());
        }
    }
}

void historyNoteMainStateValue(
    HistoryStore *store,
    DeviceTopicMap *deviceMap,
    const uint8_t ieee[8],
    uint8_t channel,
    const char *valueText
) {
    if (store == nullptr || deviceMap == nullptr || ieee == nullptr || valueText == nullptr
        || !store->available()) {
        return;
    }
    DeviceTopicEntry *entry = deviceMap->findByIeee(ieee);
    if (!deviceHistoryEnabled(entry)) {
        return;
    }
    HistorySeriesKey key{};
    memcpy(key.ieee, ieee, 8);
    key.kind = HistorySeriesMainState;
    key.channel = channel == 0 ? 1 : channel;
    if (entry->zigbeeType == ZigbeeDeviceTypeWindowCovering
        || zigbeeDeviceTypeIsMeasurement(entry->zigbeeType)) {
        char *endPointer = nullptr;
        const float numericValue = strtof(valueText, &endPointer);
        if (endPointer == valueText) {
            return;
        }
        while (*endPointer == ' ' || *endPointer == '\t') {
            endPointer++;
        }
        if (*endPointer != '\0' && *endPointer != '%') {
            return;
        }
        store->noteFloat(key, numericValue, entry->historyEpsilon, HistoryValueFloat);
        return;
    }
    const bool asOn =
        strcasecmp(valueText, "on") == 0 || strcasecmp(valueText, "ON") == 0
        || strcmp(valueText, "1") == 0;
    const bool asOff =
        strcasecmp(valueText, "off") == 0 || strcasecmp(valueText, "OFF") == 0
        || strcmp(valueText, "0") == 0;
    if (!asOn && !asOff) {
        return;
    }
    store->noteBool(key, asOn);
}
#endif // !HISTORY_STORE_UNIT_TEST

uint16_t HistoryStore::maxPagesForEntrySize(uint16_t entrySizeBytes) {
    if (entrySizeBytes <= HISTORY_ENTRY_HEADER_BYTES) {
        return 0;
    }
    return (uint16_t)((entrySizeBytes - HISTORY_ENTRY_HEADER_BYTES) / HISTORY_PAGE_RECORD_BYTES);
}

uint32_t HistoryStore::unixToEpoch2010(time_t unixSec) {
    if (unixSec < (time_t)HISTORY_EPOCH_UNIX_OFFSET) {
        return 0;
    }
    return (uint32_t)((uint32_t)unixSec - HISTORY_EPOCH_UNIX_OFFSET);
}

time_t HistoryStore::epoch2010ToUnix(uint32_t epoch2010) {
    return (time_t)(epoch2010 + HISTORY_EPOCH_UNIX_OFFSET);
}

bool HistoryStore::clockIsSane(time_t unixSec) {
    return unixSec >= kSaneUnixFloor;
}

bool HistoryStore::begin(uint16_t maxSizeKiB, uint16_t entrySizeBytes) {
    end();
    entrySize = entrySizeBytes;
    if (!historyIsValidEntrySize(entrySize)) {
        entrySize = DEFAULT_HISTORY_ENTRY_SIZE;
    }
    maxPagesPerEntry = maxPagesForEntrySize(entrySize);
    if (maxPagesPerEntry == 0) {
        return false;
    }
    if (!allocSlab(maxSizeKiB)) {
        return false;
    }
    seriesCapacity = kSeriesCapacityMax;
    series = (SeriesRuntime *)heap_caps_calloc(
        seriesCapacity,
        sizeof(SeriesRuntime),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
    if (series == nullptr) {
        freeSlab();
        return false;
    }
    seriesCount = 0;
    valuesStored = 0;
    lastSweepMs = millis();
    return true;
}

void HistoryStore::end() {
    if (series != nullptr) {
        for (uint16_t index = 0; index < seriesCapacity; index++) {
            if (series[index].entryBytes != nullptr) {
                heap_caps_free(series[index].entryBytes);
                series[index].entryBytes = nullptr;
            }
        }
        heap_caps_free(series);
        series = nullptr;
    }
    seriesCapacity = 0;
    seriesCount = 0;
    freeSlab();
}

void HistoryStore::setRetention(uint8_t maxDays, uint8_t sweepMinutes) {
    maxStoreDays = historyClampMaxStoreDays(maxDays);
    sweepIntervalMin = historyClampSweepIntervalMin(sweepMinutes);
}

bool HistoryStore::allocSlab(uint16_t maxSizeKiB) {
    const size_t bytes = (size_t)maxSizeKiB * 1024u;
    pageCount = (uint16_t)(bytes / HISTORY_PAGE_BYTES);
    if (pageCount == 0) {
        return false;
    }
    slab = (uint8_t *)heap_caps_malloc(
        (size_t)pageCount * HISTORY_PAGE_BYTES,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
    if (slab == nullptr) {
        pageCount = 0;
        return false;
    }
    const size_t bitmapBytes = ((size_t)pageCount + 7u) / 8u;
    freeBitmap = (uint8_t *)heap_caps_malloc(bitmapBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (freeBitmap == nullptr) {
        heap_caps_free(slab);
        slab = nullptr;
        pageCount = 0;
        return false;
    }
    memset(freeBitmap, 0xFF, bitmapBytes);
    return true;
}

void HistoryStore::freeSlab() {
    if (freeBitmap != nullptr) {
        heap_caps_free(freeBitmap);
        freeBitmap = nullptr;
    }
    if (slab != nullptr) {
        heap_caps_free(slab);
        slab = nullptr;
    }
    pageCount = 0;
}

int16_t HistoryStore::acquirePage() {
    if (slab == nullptr || freeBitmap == nullptr) {
        return -1;
    }
    for (uint16_t slot = 0; slot < pageCount; slot++) {
        const uint16_t byteIndex = slot / 8;
        const uint8_t bitMask = (uint8_t)(1u << (slot % 8));
        if ((freeBitmap[byteIndex] & bitMask) != 0) {
            freeBitmap[byteIndex] = (uint8_t)(freeBitmap[byteIndex] & (uint8_t)~bitMask);
            memset(pagePtr(slot), 0, HISTORY_PAGE_BYTES);
            return (int16_t)slot;
        }
    }
    return -1;
}

void HistoryStore::releasePage(uint16_t slot) {
    if (slot >= pageCount || freeBitmap == nullptr) {
        return;
    }
    const uint16_t byteIndex = slot / 8;
    const uint8_t bitMask = (uint8_t)(1u << (slot % 8));
    freeBitmap[byteIndex] = (uint8_t)(freeBitmap[byteIndex] | bitMask);
}

uint32_t HistoryStore::countValuesInPages() const {
    // Values stored = samples currently held in allocated history pages (not lifetime).
    uint32_t total = 0;
    if (slab == nullptr || freeBitmap == nullptr || pageCount == 0) {
        return 0;
    }
    for (uint16_t slot = 0; slot < pageCount; slot++) {
        const uint16_t byteIndex = slot / 8;
        const uint8_t bitMask = (uint8_t)(1u << (slot % 8));
        if ((freeBitmap[byteIndex] & bitMask) != 0) {
            continue;
        }
        const PageHeader *pageHeader = (const PageHeader *)pagePtr(slot);
        if (pageHeader != nullptr) {
            total += pageHeader->recordsCount;
        }
    }
    return total;
}

void HistoryStore::refreshValuesStored() {
    valuesStored = countValuesInPages();
}

uint8_t *HistoryStore::pagePtr(uint16_t slot) const {
    if (slab == nullptr || slot >= pageCount) {
        return nullptr;
    }
    return slab + ((size_t)slot * HISTORY_PAGE_BYTES);
}

HistoryStore::EntryHeader *HistoryStore::entryHeader(SeriesRuntime *runtime) const {
    if (runtime == nullptr || runtime->entryBytes == nullptr) {
        return nullptr;
    }
    return (EntryHeader *)runtime->entryBytes;
}

HistoryStore::PageRecord *HistoryStore::entryPageRecords(SeriesRuntime *runtime) const {
    if (runtime == nullptr || runtime->entryBytes == nullptr) {
        return nullptr;
    }
    return (PageRecord *)(runtime->entryBytes + HISTORY_ENTRY_HEADER_BYTES);
}

uint16_t HistoryStore::entryPageCount(const SeriesRuntime *runtime) const {
    if (runtime == nullptr || !runtime->used || runtime->entryBytes == nullptr) {
        return 0;
    }
    const EntryHeader *header = (const EntryHeader *)runtime->entryBytes;
    if (header->startDate == 0 && header->lastDate == 0 && header->oldestIndex == 0
        && header->newestIndex == 0) {
        const PageRecord *records = (const PageRecord *)(runtime->entryBytes + HISTORY_ENTRY_HEADER_BYTES);
        if (records[0].pageSlot == 0xFFFFFFFFu) {
            return 0;
        }
    }
    if (header->newestIndex >= maxPagesPerEntry) {
        return 0;
    }
    if (header->oldestIndex <= header->newestIndex) {
        return (uint16_t)(header->newestIndex - header->oldestIndex + 1);
    }
    return (uint16_t)(maxPagesPerEntry - header->oldestIndex + header->newestIndex + 1);
}

bool HistoryStore::keysEqual(const HistorySeriesKey &left, const HistorySeriesKey &right) const {
    if (left.kind != right.kind || memcmp(left.ieee, right.ieee, 8) != 0) {
        return false;
    }
    if (left.kind == HistorySeriesMainState) {
        return left.channel == right.channel;
    }
    return left.cluster == right.cluster && left.attribute == right.attribute;
}

HistoryStore::SeriesRuntime *HistoryStore::findSeries(const HistorySeriesKey &key) const {
    if (series == nullptr) {
        return nullptr;
    }
    for (uint16_t index = 0; index < seriesCapacity; index++) {
        if (!series[index].used) {
            continue;
        }
        if (keysEqual(series[index].key, key)) {
            return &series[index];
        }
    }
    return nullptr;
}

void HistoryStore::dropSeries(SeriesRuntime *runtime) {
    if (runtime == nullptr || !runtime->used) {
        return;
    }
    while (purgeOldestPage(runtime)) {
    }
    if (runtime->entryBytes != nullptr) {
        heap_caps_free(runtime->entryBytes);
        runtime->entryBytes = nullptr;
    }
    memset(runtime, 0, sizeof(SeriesRuntime));
    if (seriesCount > 0) {
        seriesCount--;
    }
}

bool HistoryStore::removeSeries(const HistorySeriesKey &key) {
    SeriesRuntime *runtime = findSeries(key);
    if (runtime == nullptr) {
        return false;
    }
    dropSeries(runtime);
    return true;
}

bool HistoryStore::seriesConfigured(DeviceTopicMap *deviceMap, const HistorySeriesKey &key) const {
#if defined(HISTORY_STORE_UNIT_TEST)
    (void)deviceMap;
    (void)key;
    return false;
#else
    if (deviceMap == nullptr) {
        return false;
    }
    DeviceTopicEntry *entry = deviceMap->findByIeee(key.ieee);
    if (entry == nullptr || !entry->used) {
        return false;
    }
    if (key.kind == HistorySeriesMainState) {
        if (!deviceHistoryEnabled(entry)) {
            return false;
        }
        const uint8_t channels =
            entry->channelCount == 0 ? 1 : DeviceTopicMap::normalizeChannelCount(entry->channelCount);
        return key.channel >= 1 && key.channel <= channels;
    }
    if (key.cluster == kUserDataClusterId
        && key.attribute >= kUserDataHistoryAttributeBase
        && key.attribute < (uint16_t)(kUserDataHistoryAttributeBase + kUserDataFieldMaxCount)) {
        const uint8_t fieldIndex = (uint8_t)(key.attribute - kUserDataHistoryAttributeBase);
        if (entry->userDataFields == nullptr || fieldIndex >= entry->userDataFieldCount) {
            return false;
        }
        return userDataHistoryEnabled(&entry->userDataFields[fieldIndex]);
    }
    if (entry->reportableData == nullptr || entry->reportableDataCount == 0) {
        return false;
    }
    const int rowIndex = reportableDataFindIndex(
        entry->reportableData,
        entry->reportableDataCount,
        key.cluster,
        key.attribute
    );
    if (rowIndex < 0) {
        return false;
    }
    return reportableHistoryEnabled(&entry->reportableData[rowIndex]);
#endif
}

HistoryStore::SeriesRuntime *HistoryStore::ensureSeries(
    const HistorySeriesKey &key,
    HistoryValueType valueType,
    uint8_t blockLength,
    float epsilon
) {
    SeriesRuntime *existing = findSeries(key);
    if (existing != nullptr) {
        existing->epsilon = epsilon;
        existing->valueType = valueType;
        existing->blockLength = blockLength;
        return existing;
    }
    if (!available() || series == nullptr || seriesCount >= seriesCapacity) {
        return nullptr;
    }
    SeriesRuntime *slot = nullptr;
    for (uint16_t index = 0; index < seriesCapacity; index++) {
        if (!series[index].used) {
            slot = &series[index];
            break;
        }
    }
    if (slot == nullptr) {
        return nullptr;
    }
    memset(slot, 0, sizeof(SeriesRuntime));
    slot->key = key;
    slot->valueType = valueType;
    slot->blockLength = blockLength;
    slot->epsilon = epsilon;
    slot->entryBytes = (uint8_t *)heap_caps_malloc(entrySize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (slot->entryBytes == nullptr) {
        return nullptr;
    }
    memset(slot->entryBytes, 0, entrySize);
    PageRecord *records = entryPageRecords(slot);
    for (uint16_t pageIndex = 0; pageIndex < maxPagesPerEntry; pageIndex++) {
        records[pageIndex].pageSlot = 0xFFFFFFFFu;
    }
    const int16_t firstSlot = acquirePage();
    if (firstSlot < 0) {
        heap_caps_free(slot->entryBytes);
        slot->entryBytes = nullptr;
        return nullptr;
    }
    EntryHeader *header = entryHeader(slot);
    header->startDate = 0;
    header->lastDate = 0;
    header->oldestIndex = 0;
    header->newestIndex = 0;
    records[0].startDate = 0;
    records[0].lastDate = 0;
    records[0].pageSlot = (uint32_t)firstSlot;
    PageHeader *pageHeader = (PageHeader *)pagePtr((uint16_t)firstSlot);
    pageHeader->recordsCount = 0;
    pageHeader->blockLength = blockLength;
    pageHeader->reserved = 0;
    slot->used = true;
    seriesCount++;
    return slot;
}

bool HistoryStore::purgeOldestPage(SeriesRuntime *runtime) {
    if (runtime == nullptr || !runtime->used) {
        return false;
    }
    EntryHeader *header = entryHeader(runtime);
    PageRecord *records = entryPageRecords(runtime);
    if (header == nullptr || records == nullptr) {
        return false;
    }
    const uint16_t count = entryPageCount(runtime);
    if (count == 0) {
        return false;
    }
    const uint16_t oldest = header->oldestIndex;
    if (oldest >= maxPagesPerEntry || records[oldest].pageSlot == 0xFFFFFFFFu) {
        return false;
    }
    const uint16_t pageSlot = (uint16_t)records[oldest].pageSlot;
    const PageHeader *pageHeader = (const PageHeader *)pagePtr(pageSlot);
    if (pageHeader != nullptr && valuesStored >= pageHeader->recordsCount) {
        valuesStored -= pageHeader->recordsCount;
    } else {
        valuesStored = 0;
    }
    releasePage(pageSlot);
    records[oldest].pageSlot = 0xFFFFFFFFu;
    records[oldest].startDate = 0;
    records[oldest].lastDate = 0;
    if (count == 1) {
        header->startDate = 0;
        header->lastDate = 0;
        header->oldestIndex = 0;
        header->newestIndex = 0;
        return true;
    }
    header->oldestIndex = (uint16_t)((oldest + 1) % maxPagesPerEntry);
    header->startDate = records[header->oldestIndex].startDate;
    return true;
}

void HistoryStore::purgeOutdated(SeriesRuntime *runtime, uint32_t nowEpoch2010) {
    if (runtime == nullptr || !runtime->used || maxStoreDays == 0) {
        return;
    }
    const uint32_t maxAgeSec = (uint32_t)maxStoreDays * 86400UL;
    while (true) {
        EntryHeader *header = entryHeader(runtime);
        PageRecord *records = entryPageRecords(runtime);
        if (header == nullptr || records == nullptr || entryPageCount(runtime) == 0) {
            break;
        }
        const PageRecord &oldest = records[header->oldestIndex];
        if (oldest.pageSlot == 0xFFFFFFFFu) {
            break;
        }
        if (oldest.lastDate == 0 || nowEpoch2010 < oldest.lastDate) {
            break;
        }
        if ((nowEpoch2010 - oldest.lastDate) <= maxAgeSec) {
            break;
        }
        if (!purgeOldestPage(runtime)) {
            break;
        }
    }
}

bool HistoryStore::appendRecord(
    SeriesRuntime *runtime,
    uint32_t epoch2010,
    const void *valueBytes
) {
    if (runtime == nullptr || valueBytes == nullptr || !runtime->used) {
        return false;
    }
    purgeOutdated(runtime, epoch2010);
    EntryHeader *header = entryHeader(runtime);
    PageRecord *records = entryPageRecords(runtime);
    if (header == nullptr || records == nullptr) {
        return false;
    }
    uint16_t newest = header->newestIndex;
    if (records[newest].pageSlot == 0xFFFFFFFFu) {
        const int16_t slot = acquirePage();
        if (slot < 0) {
            if (!purgeOldestPage(runtime)) {
                return false;
            }
            newest = header->newestIndex;
            if (records[newest].pageSlot == 0xFFFFFFFFu) {
                const int16_t retrySlot = acquirePage();
                if (retrySlot < 0) {
                    return false;
                }
                records[newest].pageSlot = (uint32_t)retrySlot;
                records[newest].startDate = 0;
                records[newest].lastDate = 0;
                PageHeader *retryHeader = (PageHeader *)pagePtr((uint16_t)retrySlot);
                retryHeader->recordsCount = 0;
                retryHeader->blockLength = runtime->blockLength;
                retryHeader->reserved = 0;
            }
        } else {
            records[newest].pageSlot = (uint32_t)slot;
            records[newest].startDate = 0;
            records[newest].lastDate = 0;
            PageHeader *newHeader = (PageHeader *)pagePtr((uint16_t)slot);
            newHeader->recordsCount = 0;
            newHeader->blockLength = runtime->blockLength;
            newHeader->reserved = 0;
        }
    }

    auto writeIntoPage = [&](uint16_t pageIndex) -> bool {
        PageRecord &record = records[pageIndex];
        uint8_t *page = pagePtr((uint16_t)record.pageSlot);
        if (page == nullptr) {
            return false;
        }
        PageHeader *pageHeader = (PageHeader *)page;
        if (pageHeader->blockLength == 0) {
            pageHeader->blockLength = runtime->blockLength;
        }
        const uint16_t recordBytes = (uint16_t)(4u + pageHeader->blockLength);
        const size_t used = sizeof(PageHeader) + (size_t)pageHeader->recordsCount * recordBytes;
        if (used + recordBytes > HISTORY_PAGE_BYTES) {
            return false;
        }
        uint8_t *dest = page + used;
        memcpy(dest, &epoch2010, 4);
        memcpy(dest + 4, valueBytes, pageHeader->blockLength);
        pageHeader->recordsCount++;
        if (record.startDate == 0) {
            record.startDate = epoch2010;
        }
        record.lastDate = epoch2010;
        if (header->startDate == 0) {
            header->startDate = epoch2010;
        }
        header->lastDate = epoch2010;
        valuesStored++;
        return true;
    };

    if (writeIntoPage(newest)) {
        return true;
    }

    // Need a new page.
    purgeOutdated(runtime, epoch2010);
    const uint16_t usedPages = entryPageCount(runtime);
    if (usedPages >= maxPagesPerEntry) {
        if (!purgeOldestPage(runtime)) {
            return false;
        }
    }
    int16_t newSlot = acquirePage();
    while (newSlot < 0) {
        if (!purgeOldestPage(runtime)) {
            return false;
        }
        newSlot = acquirePage();
    }
    uint16_t nextIndex = (uint16_t)((header->newestIndex + 1) % maxPagesPerEntry);
    if (records[nextIndex].pageSlot != 0xFFFFFFFFu) {
        if (!purgeOldestPage(runtime)) {
            releasePage((uint16_t)newSlot);
            return false;
        }
        nextIndex = (uint16_t)((header->newestIndex + 1) % maxPagesPerEntry);
    }
    records[nextIndex].pageSlot = (uint32_t)newSlot;
    records[nextIndex].startDate = 0;
    records[nextIndex].lastDate = 0;
    PageHeader *pageHeader = (PageHeader *)pagePtr((uint16_t)newSlot);
    pageHeader->recordsCount = 0;
    pageHeader->blockLength = runtime->blockLength;
    pageHeader->reserved = 0;
    header->newestIndex = nextIndex;
    return writeIntoPage(nextIndex);
}

bool HistoryStore::noteFloat(
    const HistorySeriesKey &key,
    float value,
    float epsilon,
    HistoryValueType valueType
) {
    const time_t nowUnix = historyWallClockUnix();
    if (!clockIsSane(nowUnix)) {
        return false;
    }
    SeriesRuntime *runtime = ensureSeries(key, valueType, sizeof(float), epsilon);
    if (runtime == nullptr) {
        return false;
    }
    if (runtime->hasLast && !floatChangedBeyond(runtime->lastFloat, value, epsilon)) {
        return false;
    }
    const uint32_t epoch2010 = unixToEpoch2010(nowUnix);
    if (!appendRecord(runtime, epoch2010, &value)) {
        return false;
    }
    runtime->hasLast = true;
    runtime->lastFloat = value;
    return true;
}

bool HistoryStore::noteInt(const HistorySeriesKey &key, int32_t value, float epsilon) {
    const time_t nowUnix = historyWallClockUnix();
    if (!clockIsSane(nowUnix)) {
        return false;
    }
    SeriesRuntime *runtime = ensureSeries(key, HistoryValueInt, sizeof(int32_t), epsilon);
    if (runtime == nullptr) {
        return false;
    }
    if (runtime->hasLast) {
        if (epsilon < 0.0f) {
            return false;
        }
        if (epsilon == 0.0f) {
            if (runtime->lastInt == value) {
                return false;
            }
        } else if (abs(runtime->lastInt - value) < (int32_t)epsilon) {
            return false;
        }
    }
    const uint32_t epoch2010 = unixToEpoch2010(nowUnix);
    if (!appendRecord(runtime, epoch2010, &value)) {
        return false;
    }
    runtime->hasLast = true;
    runtime->lastInt = value;
    return true;
}

bool HistoryStore::noteBool(const HistorySeriesKey &key, bool value) {
    const time_t nowUnix = historyWallClockUnix();
    if (!clockIsSane(nowUnix)) {
        return false;
    }
    SeriesRuntime *runtime = ensureSeries(key, HistoryValueBool, 1, 0.0f);
    if (runtime == nullptr) {
        return false;
    }
    if (runtime->hasLast && runtime->lastBool == value) {
        return false;
    }
    const uint8_t byteValue = value ? 1 : 0;
    const uint32_t epoch2010 = unixToEpoch2010(nowUnix);
    if (!appendRecord(runtime, epoch2010, &byteValue)) {
        return false;
    }
    runtime->hasLast = true;
    runtime->lastBool = value;
    return true;
}

bool HistoryStore::noteString(const HistorySeriesKey &key, const char *value) {
    const time_t nowUnix = historyWallClockUnix();
    if (value == nullptr || !clockIsSane(nowUnix)) {
        return false;
    }
    SeriesRuntime *runtime =
        ensureSeries(key, HistoryValueString, (uint8_t)kHistoryValueTextMaxChars, 0.0f);
    if (runtime == nullptr) {
        return false;
    }
    if (runtime->hasLast && strncmp(runtime->lastString, value, kHistoryValueTextMaxChars) == 0) {
        return false;
    }
    char packed[kHistoryValueTextMaxChars];
    memset(packed, 0, sizeof(packed));
    strncpy(packed, value, sizeof(packed));
    const uint32_t epoch2010 = unixToEpoch2010(nowUnix);
    if (!appendRecord(runtime, epoch2010, packed)) {
        return false;
    }
    runtime->hasLast = true;
    strncpy(runtime->lastString, packed, sizeof(runtime->lastString) - 1);
    runtime->lastString[sizeof(runtime->lastString) - 1] = '\0';
    return true;
}

void HistoryStore::syncSeriesFromDevices(DeviceTopicMap *deviceMap) {
#if defined(HISTORY_STORE_UNIT_TEST)
    (void)deviceMap;
#else
    if (deviceMap == nullptr || !available()) {
        return;
    }
    for (DeviceTopicEntry *entry = deviceMap->first(); entry != nullptr;
         entry = DeviceTopicMap::nextEntry(entry)) {
        if (!entry->used) {
            continue;
        }
        if (deviceHistoryEnabled(entry)) {
            const uint8_t channels =
                entry->channelCount == 0 ? 1 : DeviceTopicMap::normalizeChannelCount(entry->channelCount);
            for (uint8_t channel = 1; channel <= channels; channel++) {
                HistorySeriesKey key{};
                memcpy(key.ieee, entry->ieee, 8);
                key.kind = HistorySeriesMainState;
                key.channel = channel;
                const bool isFloatMain =
                    entry->zigbeeType == ZigbeeDeviceTypeWindowCovering
                    || zigbeeDeviceTypeIsMeasurement(entry->zigbeeType);
                ensureSeries(
                    key,
                    isFloatMain ? HistoryValueFloat : HistoryValueBool,
                    isFloatMain ? (uint8_t)sizeof(float) : (uint8_t)1,
                    entry->historyEpsilon
                );
            }
        }
        for (uint8_t rowIndex = 0; rowIndex < entry->reportableDataCount; rowIndex++) {
            const ReportableDataDef &row = entry->reportableData[rowIndex];
            if (!reportableHistoryEnabled(&row)) {
                continue;
            }
            HistorySeriesKey key{};
            memcpy(key.ieee, entry->ieee, 8);
            key.kind = HistorySeriesReportable;
            key.cluster = row.cluster;
            key.attribute = row.attribute;
            HistoryValueType valueType = HistoryValueString;
            uint8_t blockLength = (uint8_t)kReportableValueTextMaxChars;
            if (row.type == ReportableDataTypeFloat) {
                valueType = HistoryValueFloat;
                blockLength = sizeof(float);
            } else if (row.type == ReportableDataTypeInt) {
                valueType = HistoryValueInt;
                blockLength = sizeof(int32_t);
            } else if (row.type == ReportableDataTypeBool) {
                valueType = HistoryValueBool;
                blockLength = 1;
            }
            ensureSeries(key, valueType, blockLength, row.epsilon);
        }
        if (entry->userDataFields != nullptr) {
            for (uint8_t fieldIndex = 0; fieldIndex < entry->userDataFieldCount; fieldIndex++) {
                const UserDataFieldDef &field = entry->userDataFields[fieldIndex];
                if (!userDataHistoryEnabled(&field)) {
                    continue;
                }
                HistorySeriesKey key{};
                memcpy(key.ieee, entry->ieee, 8);
                key.kind = HistorySeriesReportable;
                key.cluster = kUserDataClusterId;
                key.attribute = userDataHistoryAttributeForFieldIndex(fieldIndex);
                HistoryValueType valueType = HistoryValueString;
                uint8_t blockLength = (uint8_t)kReportableValueTextMaxChars;
                const UserDataFieldType fieldType = (UserDataFieldType)field.type;
                if (fieldType == UserDataFieldTypeFloat) {
                    valueType = HistoryValueFloat;
                    blockLength = sizeof(float);
                } else if (fieldType == UserDataFieldTypeInt) {
                    valueType = HistoryValueInt;
                    blockLength = sizeof(int32_t);
                } else if (fieldType == UserDataFieldTypeBool) {
                    valueType = HistoryValueBool;
                    blockLength = 1;
                }
                ensureSeries(key, valueType, blockLength, field.epsilon);
            }
        }
    }
    if (series == nullptr) {
        return;
    }
    for (uint16_t index = 0; index < seriesCapacity; index++) {
        if (!series[index].used) {
            continue;
        }
        if (!seriesConfigured(deviceMap, series[index].key)) {
            dropSeries(&series[index]);
        }
    }
#endif
}

void HistoryStore::setSdBackend(SdCardStorage *sdStorage) {
    sdBackend = sdStorage;
    if (persistOnSd()) {
        lastSdBackupMs = millis();
    }
}

bool HistoryStore::persistOnSd() const {
#if defined(HISTORY_STORE_UNIT_TEST) || !defined(BOARD_HOST_P4)
    (void)sdBackend;
    return false;
#else
    return sdBackend != nullptr && sdBackend->usable();
#endif
}

void HistoryStore::setSdBackupIntervalMin(uint16_t minutes) {
    if (minutes < HISTORY_SD_BACKUP_INTERVAL_MIN_MIN) {
        minutes = HISTORY_SD_BACKUP_INTERVAL_MIN_MIN;
    }
    if (minutes > HISTORY_SD_BACKUP_INTERVAL_MIN_MAX) {
        minutes = HISTORY_SD_BACKUP_INTERVAL_MIN_MAX;
    }
    sdBackupIntervalMinutes = minutes;
    if (persistOnSd()) {
        lastSdBackupMs = millis();
    }
}

uint32_t HistoryStore::sdSecondsToSave() const {
    if (!persistOnSd()) {
        return 0;
    }
    const unsigned long intervalMs =
        (unsigned long)sdBackupIntervalMinutes * 60000UL;
    const unsigned long elapsed = millis() - lastSdBackupMs;
    if (elapsed >= intervalMs) {
        return 0;
    }
    return (uint32_t)((intervalMs - elapsed + 999UL) / 1000UL);
}

void HistoryStore::loop() {
    if (!available()) {
        return;
    }
    const unsigned long intervalMs = (unsigned long)sweepIntervalMin * 60000UL;
    if (millis() - lastSweepMs >= intervalMs) {
        lastSweepMs = millis();
        sweepAll();
    }
#if !defined(HISTORY_STORE_UNIT_TEST) && defined(BOARD_HOST_P4)
    if (persistOnSd()) {
        const unsigned long backupIntervalMs =
            (unsigned long)sdBackupIntervalMinutes * 60000UL;
        if (millis() - lastSdBackupMs >= backupIntervalMs) {
            LOGGER.info("History SD periodic backup start");
            if (persistSnapshot()) {
                lastSdBackupMs = millis();
                LOGGER.info("History SD periodic backup ok");
            } else {
                lastSdBackupMs = millis();
                LOGGER.warning("History SD periodic backup failed");
            }
        }
    }
#endif
}

void HistoryStore::sweepAll() {
    const time_t nowUnix = historyWallClockUnix();
    if (!clockIsSane(nowUnix)) {
        return;
    }
    const uint32_t nowEpoch = unixToEpoch2010(nowUnix);
    for (uint16_t index = 0; index < seriesCapacity; index++) {
        if (!series[index].used) {
            continue;
        }
        purgeOutdated(&series[index], nowEpoch);
    }
}

void HistoryStore::forEachSeriesWithLastSample(
    void (*visitor)(
        const HistorySeriesKey &key,
        HistoryValueType valueType,
        const HistorySample &sample,
        void *context
    ),
    void *context
) const {
    if (visitor == nullptr || series == nullptr || !available()) {
        return;
    }
    for (uint16_t index = 0; index < seriesCapacity; index++) {
        const SeriesRuntime &runtime = series[index];
        if (!runtime.used || !runtime.hasLast) {
            continue;
        }
        HistorySample sample{};
        sample.valueType = runtime.valueType;
        sample.floatValue = runtime.lastFloat;
        sample.intValue = runtime.lastInt;
        sample.boolValue = runtime.lastBool;
        memcpy(sample.stringValue, runtime.lastString, kHistoryValueTextMaxLen);
        sample.stringValue[kHistoryValueTextMaxChars] = '\0';
        visitor(runtime.key, runtime.valueType, sample, context);
    }
}

bool HistoryStore::query(
    const HistorySeriesKey &key,
    uint32_t fromEpoch2010,
    uint32_t toEpoch2010,
    uint16_t maxPoints,
    HistorySample *outSamples,
    uint16_t *outCount,
    HistoryValueType *outType
) const {
    if (outSamples == nullptr || outCount == nullptr) {
        return false;
    }
    *outCount = 0;
    const SeriesRuntime *runtime = findSeries(key);
    if (runtime == nullptr || !runtime->used) {
        return true;
    }
    if (outType != nullptr) {
        *outType = runtime->valueType;
    }
    if (maxPoints == 0) {
        return true;
    }
    const EntryHeader *header = (const EntryHeader *)runtime->entryBytes;
    const PageRecord *records =
        (const PageRecord *)(runtime->entryBytes + HISTORY_ENTRY_HEADER_BYTES);
    const uint16_t pageCountLocal = entryPageCount(runtime);
    if (pageCountLocal == 0) {
        return true;
    }

    auto fillSample = [&](HistorySample *sample, uint32_t epoch2010, const uint8_t *valueBytes) {
        memset(sample, 0, sizeof(HistorySample));
        sample->epoch2010 = epoch2010;
        sample->valueType = runtime->valueType;
        if (runtime->valueType == HistoryValueFloat) {
            memcpy(&sample->floatValue, valueBytes, sizeof(float));
        } else if (runtime->valueType == HistoryValueInt) {
            memcpy(&sample->intValue, valueBytes, sizeof(int32_t));
        } else if (runtime->valueType == HistoryValueBool) {
            sample->boolValue = valueBytes[0] != 0;
        } else {
            memcpy(sample->stringValue, valueBytes, runtime->blockLength);
            sample->stringValue[sizeof(sample->stringValue) - 1] = '\0';
        }
    };

    // Pass 1: count in-range samples (no large stack buffer).
    uint32_t matchCount = 0;
    uint16_t pageIndex = header->oldestIndex;
    for (uint16_t seen = 0; seen < pageCountLocal; seen++) {
        const PageRecord &record = records[pageIndex];
        if (record.pageSlot != 0xFFFFFFFFu && record.pageSlot < pageCount) {
            uint8_t *page = pagePtr((uint16_t)record.pageSlot);
            if (page != nullptr) {
                PageHeader *pageHeader = (PageHeader *)page;
                const uint8_t blockLength = pageHeader->blockLength;
                if (blockLength == 0 || blockLength > kReportableValueTextMaxChars) {
                    pageIndex = (uint16_t)((pageIndex + 1) % maxPagesPerEntry);
                    continue;
                }
                const uint16_t recordBytes = (uint16_t)(4u + blockLength);
                const uint16_t maxRecords =
                    (uint16_t)((HISTORY_PAGE_BYTES - sizeof(PageHeader)) / recordBytes);
                const uint16_t safeCount =
                    pageHeader->recordsCount > maxRecords ? maxRecords : pageHeader->recordsCount;
                for (uint16_t recordIndex = 0; recordIndex < safeCount; recordIndex++) {
                    const uint8_t *recordPtr =
                        page + sizeof(PageHeader) + (size_t)recordIndex * recordBytes;
                    uint32_t epoch2010 = 0;
                    memcpy(&epoch2010, recordPtr, 4);
                    if (epoch2010 >= fromEpoch2010 && epoch2010 <= toEpoch2010) {
                        matchCount++;
                    }
                }
            }
        }
        pageIndex = (uint16_t)((pageIndex + 1) % maxPagesPerEntry);
    }
    if (matchCount == 0) {
        return true;
    }

    const uint16_t outN =
        matchCount <= (uint32_t)maxPoints ? (uint16_t)matchCount : maxPoints;
    uint32_t matchIndex = 0;
    uint16_t outIndex = 0;
    uint32_t nextTake = 0;
    pageIndex = header->oldestIndex;
    for (uint16_t seen = 0; seen < pageCountLocal && outIndex < outN; seen++) {
        const PageRecord &record = records[pageIndex];
        if (record.pageSlot != 0xFFFFFFFFu && record.pageSlot < pageCount) {
            uint8_t *page = pagePtr((uint16_t)record.pageSlot);
            if (page != nullptr) {
                PageHeader *pageHeader = (PageHeader *)page;
                const uint8_t blockLength = pageHeader->blockLength;
                if (blockLength == 0 || blockLength > kReportableValueTextMaxChars) {
                    pageIndex = (uint16_t)((pageIndex + 1) % maxPagesPerEntry);
                    continue;
                }
                const uint16_t recordBytes = (uint16_t)(4u + blockLength);
                const uint16_t maxRecords =
                    (uint16_t)((HISTORY_PAGE_BYTES - sizeof(PageHeader)) / recordBytes);
                const uint16_t safeCount =
                    pageHeader->recordsCount > maxRecords ? maxRecords : pageHeader->recordsCount;
                for (uint16_t recordIndex = 0; recordIndex < safeCount; recordIndex++) {
                    const uint8_t *recordPtr =
                        page + sizeof(PageHeader) + (size_t)recordIndex * recordBytes;
                    uint32_t epoch2010 = 0;
                    memcpy(&epoch2010, recordPtr, 4);
                    if (epoch2010 < fromEpoch2010 || epoch2010 > toEpoch2010) {
                        continue;
                    }
                    const bool take =
                        (outN == (uint16_t)matchCount)
                        || (matchIndex == nextTake)
                        || (outIndex + 1 == outN && matchIndex + 1 == matchCount);
                    if (take && outIndex < outN) {
                        fillSample(&outSamples[outIndex], epoch2010, recordPtr + 4);
                        outIndex++;
                        if (outN > 1 && matchCount > 1) {
                            nextTake = (uint32_t)outIndex * (matchCount - 1) / (outN - 1);
                        }
                    }
                    matchIndex++;
                }
            }
        }
        pageIndex = (uint16_t)((pageIndex + 1) % maxPagesPerEntry);
    }
    *outCount = outIndex;
    return true;
}

HistoryStoreStats HistoryStore::stats() const {
    HistoryStoreStats result{};
    result.entriesCount = seriesCount;
    result.pagesTotal = pageCount;
    result.valuesStored = countValuesInPages();
    uint16_t used = 0;
    if (freeBitmap != nullptr) {
        for (uint16_t slot = 0; slot < pageCount; slot++) {
            const uint16_t byteIndex = slot / 8;
            const uint8_t bitMask = (uint8_t)(1u << (slot % 8));
            if ((freeBitmap[byteIndex] & bitMask) == 0) {
                used++;
            }
        }
    }
    result.pagesUsed = used;
    result.slabUsedKiB = ((uint32_t)used * HISTORY_PAGE_BYTES) / 1024u;
    return result;
}

uint32_t HistoryStore::crc32Update(uint32_t crc, const uint8_t *data, size_t len) const {
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            const uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return crc;
}

uint32_t HistoryStore::crc32Finish(uint32_t crc) const {
    return ~crc;
}

bool HistoryStore::littleFsHasConfigSafeSpace(size_t extraBytes) const {
#if defined(HISTORY_STORE_UNIT_TEST)
    (void)extraBytes;
    return true;
#else
    const size_t total = LittleFS.totalBytes();
    const size_t used = LittleFS.usedBytes();
    const size_t freeBytes = total > used ? (total - used) : 0;
    return freeBytes >= (extraBytes + kSnapshotConfigHeadroomBytes);
#endif
}

bool HistoryStore::snapshotFilePresent() const {
#if defined(HISTORY_STORE_UNIT_TEST)
    return false;
#else
#if defined(BOARD_HOST_P4)
    if (persistOnSd()) {
        if (sdBackend == nullptr) {
            return false;
        }
        if (!sdBackend->remount()) {
            return false;
        }
        const bool present = SD_MMC.exists(kSnapshotPath);
        if (present) {
            File snap = SD_MMC.open(kSnapshotPath, "r");
            if (snap) {
                sdBackend->setHistorySnapshotBytes(snap.size());
                snap.close();
            }
        } else {
            sdBackend->clearHistorySnapshotSize();
        }
        sdBackend->refreshSizeCache();
        sdBackend->unmount();
        return present;
    }
#endif
    return LittleFS.exists(kSnapshotPath);
#endif
}

void HistoryStore::discardSnapshotFile() {
#if !defined(HISTORY_STORE_UNIT_TEST)
#if defined(BOARD_HOST_P4)
    if (persistOnSd()) {
        if (sdBackend == nullptr || !sdBackend->remount()) {
            return;
        }
        if (SD_MMC.exists(kSnapshotTempPath)) {
            SD_MMC.remove(kSnapshotTempPath);
        }
        if (SD_MMC.exists(kSnapshotPath)) {
            SD_MMC.remove(kSnapshotPath);
        }
        sdBackend->clearHistorySnapshotSize();
        sdBackend->refreshSizeCache();
        sdBackend->unmount();
        return;
    }
#endif
    if (LittleFS.exists(kSnapshotTempPath)) {
        LittleFS.remove(kSnapshotTempPath);
    }
    if (LittleFS.exists(kSnapshotPath)) {
        LittleFS.remove(kSnapshotPath);
    }
#endif
}

bool HistoryStore::writeSnapshotFile(const uint8_t *bytes, size_t len) const {
#if defined(HISTORY_STORE_UNIT_TEST)
    (void)bytes;
    (void)len;
    return false;
#else
    if (bytes == nullptr || len == 0) {
        return false;
    }
#if defined(BOARD_HOST_P4)
    if (persistOnSd()) {
        if (sdBackend == nullptr) {
            return false;
        }
        LOGGER.info("History SD save: mount then 3-step commit");
        if (!sdBackend->remount()) {
            return false;
        }
        SD_MMC.mkdir(kSnapshotDir);

        // Step 1: write all temp files. On any failure, leave old files intact.
        File out = SD_MMC.open(kSnapshotTempPath, "w");
        if (!out) {
            LOGGER.warning("History SD step1 open temp failed; old files kept");
            sdBackend->unmount();
            return false;
        }
        const size_t written = out.write(bytes, len);
        out.flush();
        out.close();
        if (written != len) {
            SD_MMC.remove(kSnapshotTempPath);
            LOGGER.warning("History SD step1 write incomplete; old files kept");
            sdBackend->unmount();
            return false;
        }
        {
            File verify = SD_MMC.open(kSnapshotTempPath, "r");
            const size_t tempSize = verify ? verify.size() : 0;
            if (verify) {
                verify.close();
            }
            if (tempSize != len) {
                SD_MMC.remove(kSnapshotTempPath);
                LOGGER.warning("History SD step1 size check failed; old files kept");
                sdBackend->unmount();
                return false;
            }
        }
        LOGGER.info("History SD step1 temp write ok");

        // Step 2: only after all temps succeeded — remove old normal files.
        if (SD_MMC.exists(kSnapshotPath)) {
            if (!SD_MMC.remove(kSnapshotPath)) {
                LOGGER.warning("History SD step2 remove old failed; keeping temp");
                sdBackend->unmount();
                return false;
            }
        }
        LOGGER.info("History SD step2 old files removed");

        // Step 3: rename temp → normal. Keep temp on failure so data is not lost.
        if (!SD_MMC.rename(kSnapshotTempPath, kSnapshotPath)) {
            LOGGER.error("History SD step3 rename failed; temp left on card");
            sdBackend->unmount();
            return false;
        }
        LOGGER.info("History SD step3 rename ok; unmount");
        sdBackend->setHistorySnapshotBytes(len);
        sdBackend->refreshSizeCache();
        sdBackend->unmount();
        return true;
    }
#endif
    if (!littleFsHasConfigSafeSpace(len)) {
        return false;
    }
    LittleFS.mkdir(kSnapshotDir);
    File out = LittleFS.open(kSnapshotTempPath, "w");
    if (!out) {
        return false;
    }
    const size_t written = out.write(bytes, len);
    out.flush();
    out.close();
    if (written != len) {
        LittleFS.remove(kSnapshotTempPath);
        return false;
    }
    if (LittleFS.exists(kSnapshotPath)) {
        LittleFS.remove(kSnapshotPath);
    }
    if (!LittleFS.rename(kSnapshotTempPath, kSnapshotPath)) {
        LittleFS.remove(kSnapshotTempPath);
        return false;
    }
    return true;
#endif
}

bool HistoryStore::readSnapshotFile(uint8_t **outBytes, size_t *outLen) const {
#if defined(HISTORY_STORE_UNIT_TEST)
    (void)outBytes;
    (void)outLen;
    return false;
#else
    if (outBytes == nullptr || outLen == nullptr) {
        return false;
    }
    *outBytes = nullptr;
    *outLen = 0;
#if defined(BOARD_HOST_P4)
    if (persistOnSd()) {
        if (sdBackend == nullptr || !sdBackend->remount()) {
            return false;
        }
        File in = SD_MMC.open(kSnapshotPath, "r");
        if (!in) {
            sdBackend->clearHistorySnapshotSize();
            sdBackend->refreshSizeCache();
            sdBackend->unmount();
            return false;
        }
        const size_t len = in.size();
        if (len < sizeof(SnapshotFileHeader)) {
            in.close();
            sdBackend->clearHistorySnapshotSize();
            sdBackend->refreshSizeCache();
            sdBackend->unmount();
            return false;
        }
        uint8_t *bytes = (uint8_t *)heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (bytes == nullptr) {
            bytes = (uint8_t *)malloc(len);
        }
        if (bytes == nullptr) {
            in.close();
            sdBackend->unmount();
            return false;
        }
        const size_t got = in.read(bytes, len);
        in.close();
        if (got != len) {
            free(bytes);
            sdBackend->unmount();
            return false;
        }
        sdBackend->setHistorySnapshotBytes(len);
        sdBackend->refreshSizeCache();
        sdBackend->unmount();
        *outBytes = bytes;
        *outLen = len;
        return true;
    }
#endif
    File in = LittleFS.open(kSnapshotPath, "r");
    if (!in) {
        return false;
    }
    const size_t len = in.size();
    if (len < sizeof(SnapshotFileHeader)) {
        in.close();
        return false;
    }
    uint8_t *bytes = (uint8_t *)heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (bytes == nullptr) {
        bytes = (uint8_t *)malloc(len);
    }
    if (bytes == nullptr) {
        in.close();
        return false;
    }
    const size_t got = in.read(bytes, len);
    in.close();
    if (got != len) {
        free(bytes);
        return false;
    }
    *outBytes = bytes;
    *outLen = len;
    return true;
#endif
}

bool HistoryStore::buildSnapshot(uint8_t **outBytes, size_t *outLen) const {
    if (outBytes == nullptr || outLen == nullptr || !available() || series == nullptr) {
        return false;
    }
    *outBytes = nullptr;
    *outLen = 0;

    uint16_t usedSeries = 0;
    const uint16_t mapCap = pageCount > 0 ? pageCount : 1;
    uint16_t *slotMap = (uint16_t *)malloc((size_t)mapCap * sizeof(uint16_t));
    uint32_t *oldSlots = (uint32_t *)malloc((size_t)mapCap * sizeof(uint32_t));
    if (slotMap == nullptr || oldSlots == nullptr) {
        free(slotMap);
        free(oldSlots);
        return false;
    }
    for (uint16_t i = 0; i < mapCap; i++) {
        slotMap[i] = 0xFFFFu;
    }
    uint16_t packedPageCount = 0;
    for (uint16_t index = 0; index < seriesCapacity; index++) {
        if (!series[index].used || series[index].entryBytes == nullptr) {
            continue;
        }
        usedSeries++;
        const uint16_t count = entryPageCount(&series[index]);
        if (count == 0) {
            continue;
        }
        EntryHeader *header = entryHeader(&series[index]);
        PageRecord *records = entryPageRecords(&series[index]);
        uint16_t pageIndex = header->oldestIndex;
        for (uint16_t n = 0; n < count; n++) {
            const uint32_t slot = records[pageIndex].pageSlot;
            if (slot != 0xFFFFFFFFu && slot < mapCap && slotMap[slot] == 0xFFFFu) {
                if (packedPageCount >= mapCap) {
                    free(slotMap);
                    free(oldSlots);
                    return false;
                }
                slotMap[slot] = packedPageCount;
                oldSlots[packedPageCount] = slot;
                packedPageCount++;
            }
            pageIndex = (uint16_t)((pageIndex + 1) % maxPagesPerEntry);
        }
    }

    const size_t payloadSeriesBytes =
        (size_t)usedSeries * (sizeof(SnapshotSeriesHeader) + (size_t)entrySize);
    const size_t payloadPageBytes = (size_t)packedPageCount * HISTORY_PAGE_BYTES;
    const size_t totalLen = sizeof(SnapshotFileHeader) + payloadSeriesBytes + payloadPageBytes;
    uint8_t *bytes = (uint8_t *)heap_caps_malloc(totalLen, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (bytes == nullptr) {
        bytes = (uint8_t *)malloc(totalLen);
    }
    if (bytes == nullptr) {
        free(slotMap);
        free(oldSlots);
        return false;
    }
    memset(bytes, 0, totalLen);

    SnapshotFileHeader *fileHeader = (SnapshotFileHeader *)bytes;
    fileHeader->magic = kSnapshotMagic;
    fileHeader->formatVersion = kSnapshotFormatVersion;
    fileHeader->entrySize = entrySize;
    fileHeader->pageBytes = HISTORY_PAGE_BYTES;
    fileHeader->seriesCount = usedSeries;
    fileHeader->packedPageCount = packedPageCount;
    fileHeader->reserved0 = 0;
    fileHeader->valuesStored = countValuesInPages();
    fileHeader->payloadCrc = 0;

    uint8_t *cursor = bytes + sizeof(SnapshotFileHeader);
    for (uint16_t index = 0; index < seriesCapacity; index++) {
        if (!series[index].used || series[index].entryBytes == nullptr) {
            continue;
        }
        SnapshotSeriesHeader *seriesHeader = (SnapshotSeriesHeader *)cursor;
        cursor += sizeof(SnapshotSeriesHeader);
        memcpy(seriesHeader->ieee, series[index].key.ieee, 8);
        seriesHeader->kind = (uint8_t)series[index].key.kind;
        seriesHeader->channel = series[index].key.channel;
        seriesHeader->cluster = series[index].key.cluster;
        seriesHeader->attribute = series[index].key.attribute;
        seriesHeader->valueType = (uint8_t)series[index].valueType;
        seriesHeader->blockLength = series[index].blockLength;
        seriesHeader->hasLast = series[index].hasLast ? 1 : 0;
        seriesHeader->epsilon = series[index].epsilon;
        seriesHeader->lastFloat = series[index].lastFloat;
        seriesHeader->lastInt = series[index].lastInt;
        seriesHeader->lastBool = series[index].lastBool ? 1 : 0;
        memcpy(seriesHeader->lastString, series[index].lastString, kHistoryValueTextMaxLen);

        memcpy(cursor, series[index].entryBytes, entrySize);
        PageRecord *outRecords =
            (PageRecord *)(cursor + HISTORY_ENTRY_HEADER_BYTES);
        for (uint16_t pageIndex = 0; pageIndex < maxPagesPerEntry; pageIndex++) {
            const uint32_t slot = outRecords[pageIndex].pageSlot;
            if (slot != 0xFFFFFFFFu && slot < mapCap && slotMap[slot] != 0xFFFFu) {
                outRecords[pageIndex].pageSlot = slotMap[slot];
            } else if (slot != 0xFFFFFFFFu) {
                outRecords[pageIndex].pageSlot = 0xFFFFFFFFu;
            }
        }
        cursor += entrySize;
    }

    for (uint16_t packed = 0; packed < packedPageCount; packed++) {
        const uint8_t *src = pagePtr((uint16_t)oldSlots[packed]);
        if (src == nullptr) {
            free(slotMap);
            free(oldSlots);
            free(bytes);
            return false;
        }
        memcpy(cursor, src, HISTORY_PAGE_BYTES);
        cursor += HISTORY_PAGE_BYTES;
    }

    free(slotMap);
    free(oldSlots);
    const size_t payloadLen = totalLen - sizeof(SnapshotFileHeader);
    fileHeader->payloadCrc = crc32Finish(
        crc32Update(0xFFFFFFFFu, bytes + sizeof(SnapshotFileHeader), payloadLen)
    );
    *outBytes = bytes;
    *outLen = totalLen;
    return true;
}

bool HistoryStore::applySnapshot(const uint8_t *bytes, size_t len) {
    if (bytes == nullptr || len < sizeof(SnapshotFileHeader) || !available() || series == nullptr) {
        return false;
    }
    const SnapshotFileHeader *fileHeader = (const SnapshotFileHeader *)bytes;
    if (fileHeader->magic != kSnapshotMagic || fileHeader->formatVersion != kSnapshotFormatVersion) {
        return false;
    }
    if (fileHeader->pageBytes != HISTORY_PAGE_BYTES) {
        return false;
    }
    if (!historyIsValidEntrySize(fileHeader->entrySize)) {
        return false;
    }
    const uint16_t savedEntrySize = fileHeader->entrySize;
    const uint16_t savedMaxPages = maxPagesForEntrySize(savedEntrySize);
    if (savedMaxPages == 0) {
        return false;
    }
    const size_t expected = sizeof(SnapshotFileHeader)
        + (size_t)fileHeader->seriesCount * (sizeof(SnapshotSeriesHeader) + (size_t)savedEntrySize)
        + (size_t)fileHeader->packedPageCount * HISTORY_PAGE_BYTES;
    if (len < expected) {
        return false;
    }
    const size_t payloadLen = expected - sizeof(SnapshotFileHeader);
    const uint32_t crc = crc32Finish(
        crc32Update(0xFFFFFFFFu, bytes + sizeof(SnapshotFileHeader), payloadLen)
    );
    if (crc != fileHeader->payloadCrc) {
        return false;
    }

    const uint8_t *pageBase = bytes + sizeof(SnapshotFileHeader)
        + (size_t)fileHeader->seriesCount * (sizeof(SnapshotSeriesHeader) + (size_t)savedEntrySize);

    // Drop existing series so restore owns the slab.
    for (uint16_t index = 0; index < seriesCapacity; index++) {
        if (series[index].used) {
            dropSeries(&series[index]);
        }
    }
    valuesStored = 0;

    const uint8_t *cursor = bytes + sizeof(SnapshotFileHeader);
    for (uint16_t seriesIndex = 0; seriesIndex < fileHeader->seriesCount; seriesIndex++) {
        const SnapshotSeriesHeader *seriesHeader = (const SnapshotSeriesHeader *)cursor;
        cursor += sizeof(SnapshotSeriesHeader);
        const uint8_t *savedEntry = cursor;
        cursor += savedEntrySize;

        HistorySeriesKey key{};
        memcpy(key.ieee, seriesHeader->ieee, 8);
        key.kind = (HistorySeriesKind)seriesHeader->kind;
        key.channel = seriesHeader->channel;
        key.cluster = seriesHeader->cluster;
        key.attribute = seriesHeader->attribute;

        const EntryHeader *savedHeader = (const EntryHeader *)savedEntry;
        const PageRecord *savedRecords =
            (const PageRecord *)(savedEntry + HISTORY_ENTRY_HEADER_BYTES);

        // Collect ring pages newest-first, then keep only what fits current entry size.
        uint16_t ringCount = 0;
        if (!(savedHeader->startDate == 0 && savedHeader->lastDate == 0
                && savedHeader->oldestIndex == 0 && savedHeader->newestIndex == 0
                && savedRecords[0].pageSlot == 0xFFFFFFFFu)) {
            if (savedHeader->newestIndex < savedMaxPages) {
                if (savedHeader->oldestIndex <= savedHeader->newestIndex) {
                    ringCount = (uint16_t)(savedHeader->newestIndex - savedHeader->oldestIndex + 1);
                } else {
                    ringCount =
                        (uint16_t)(savedMaxPages - savedHeader->oldestIndex + savedHeader->newestIndex
                                   + 1);
                }
            }
        }
        if (ringCount > savedMaxPages) {
            ringCount = savedMaxPages;
        }

        uint16_t keepCount = ringCount;
        if (keepCount > maxPagesPerEntry) {
            keepCount = maxPagesPerEntry;
        }

        // Walk from newest backward to pick keepCount pages.
        uint32_t keepPacked[256];
        uint32_t keepStart[256];
        uint32_t keepLast[256];
        uint16_t keepFilled = 0;
        if (ringCount > 0 && keepCount > 0) {
            uint16_t pageIndex = savedHeader->newestIndex;
            const uint16_t skipOldest = (uint16_t)(ringCount - keepCount);
            // Advance from oldest by skipOldest first, then copy keepCount toward newest.
            pageIndex = savedHeader->oldestIndex;
            for (uint16_t skip = 0; skip < skipOldest; skip++) {
                pageIndex = (uint16_t)((pageIndex + 1) % savedMaxPages);
            }
            for (uint16_t n = 0; n < keepCount; n++) {
                const PageRecord &rec = savedRecords[pageIndex];
                if (rec.pageSlot != 0xFFFFFFFFu
                    && rec.pageSlot < fileHeader->packedPageCount
                    && keepFilled < 256) {
                    keepPacked[keepFilled] = rec.pageSlot;
                    keepStart[keepFilled] = rec.startDate;
                    keepLast[keepFilled] = rec.lastDate;
                    keepFilled++;
                }
                pageIndex = (uint16_t)((pageIndex + 1) % savedMaxPages);
            }
        }

        // If slab cannot hold these pages, drop oldest of this series until fit.
        while (keepFilled > 0) {
            const HistoryStoreStats now = stats();
            const uint16_t freePages =
                now.pagesTotal > now.pagesUsed ? (uint16_t)(now.pagesTotal - now.pagesUsed) : 0;
            if (freePages >= keepFilled) {
                break;
            }
            // Drop oldest kept page.
            for (uint16_t i = 1; i < keepFilled; i++) {
                keepPacked[i - 1] = keepPacked[i];
                keepStart[i - 1] = keepStart[i];
                keepLast[i - 1] = keepLast[i];
            }
            keepFilled--;
        }
        if (keepFilled == 0 && ringCount > 0) {
            // No room for this series' pages — skip series.
            continue;
        }

        SeriesRuntime *slot = nullptr;
        for (uint16_t index = 0; index < seriesCapacity; index++) {
            if (!series[index].used) {
                slot = &series[index];
                break;
            }
        }
        if (slot == nullptr) {
            continue;
        }
        memset(slot, 0, sizeof(SeriesRuntime));
        slot->key = key;
        slot->valueType = (HistoryValueType)seriesHeader->valueType;
        slot->blockLength = seriesHeader->blockLength;
        slot->epsilon = seriesHeader->epsilon;
        slot->hasLast = seriesHeader->hasLast != 0;
        slot->lastFloat = seriesHeader->lastFloat;
        slot->lastInt = seriesHeader->lastInt;
        slot->lastBool = seriesHeader->lastBool != 0;
        memcpy(slot->lastString, seriesHeader->lastString, kHistoryValueTextMaxLen);
        slot->entryBytes = (uint8_t *)heap_caps_malloc(entrySize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (slot->entryBytes == nullptr) {
            continue;
        }
        memset(slot->entryBytes, 0, entrySize);
        PageRecord *records = entryPageRecords(slot);
        for (uint16_t pageIndex = 0; pageIndex < maxPagesPerEntry; pageIndex++) {
            records[pageIndex].pageSlot = 0xFFFFFFFFu;
        }

        EntryHeader *header = entryHeader(slot);
        header->startDate = 0;
        header->lastDate = 0;
        header->oldestIndex = 0;
        header->newestIndex = 0;

        uint16_t written = 0;
        for (uint16_t n = 0; n < keepFilled; n++) {
            const int16_t newSlot = acquirePage();
            if (newSlot < 0) {
                break;
            }
            uint8_t *dst = pagePtr((uint16_t)newSlot);
            const uint8_t *src = pageBase + ((size_t)keepPacked[n] * HISTORY_PAGE_BYTES);
            memcpy(dst, src, HISTORY_PAGE_BYTES);
            records[written].pageSlot = (uint32_t)newSlot;
            records[written].startDate = keepStart[n];
            records[written].lastDate = keepLast[n];
            if (written == 0) {
                header->startDate = keepStart[n];
                header->oldestIndex = 0;
            }
            header->lastDate = keepLast[n];
            header->newestIndex = written;
            written++;
        }
        if (written == 0) {
            // Still create an empty first page so series is usable.
            const int16_t firstSlot = acquirePage();
            if (firstSlot < 0) {
                heap_caps_free(slot->entryBytes);
                slot->entryBytes = nullptr;
                continue;
            }
            PageHeader *pageHeader = (PageHeader *)pagePtr((uint16_t)firstSlot);
            pageHeader->recordsCount = 0;
            pageHeader->blockLength = slot->blockLength;
            pageHeader->reserved = 0;
            records[0].pageSlot = (uint32_t)firstSlot;
            records[0].startDate = 0;
            records[0].lastDate = 0;
        }
        slot->used = true;
        seriesCount++;
    }
    refreshValuesStored();
    return true;
}

bool HistoryStore::persistSnapshot() {
    if (!available()) {
        return false;
    }
    uint8_t *bytes = nullptr;
    size_t len = 0;
    if (!buildSnapshot(&bytes, &len) || bytes == nullptr) {
        return false;
    }
    const bool ok = writeSnapshotFile(bytes, len);
    free(bytes);
    return ok;
}

bool HistoryStore::restoreSnapshot() {
    uint8_t *bytes = nullptr;
    size_t len = 0;
    if (!readSnapshotFile(&bytes, &len) || bytes == nullptr) {
        return false;
    }
    const bool ok = applySnapshot(bytes, len);
    free(bytes);
    if (ok) {
        // LittleFS: delete after restore. SD: keep the durable snapshot file.
        if (!persistOnSd()) {
            discardSnapshotFile();
        } else {
#if !defined(HISTORY_STORE_UNIT_TEST)
            LOGGER.info("History SD restore keep-file (no delete-after-restore)");
#endif
        }
    }
    return ok;
}

#if defined(HISTORY_STORE_UNIT_TEST)
bool HistoryStore::exportSnapshotToHeap(uint8_t **outBytes, size_t *outLen) const {
    return buildSnapshot(outBytes, outLen);
}

bool HistoryStore::importSnapshotFromHeap(const uint8_t *bytes, size_t len) {
    return applySnapshot(bytes, len);
}
#endif
