#include "HistoryStore.h"

#include "SettingsManager.h"
#include "UserDataPayload.h"
#include "ZigbeeDeviceType.h"

#include <esp_heap_caps.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace {

constexpr size_t kSeriesCapacityMax = 256;
constexpr time_t kSaneUnixFloor = 1700000000;

bool floatChangedBeyond(float previous, float next, float epsilon) {
    if (epsilon <= 0.0f) {
        return previous != next;
    }
    return fabsf(next - previous) >= epsilon;
}

} // namespace

HistoryStore::HistoryStore() = default;

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
    if (entry->zigbeeType == ZigbeeDeviceTypeWindowCovering) {
        char *endPointer = nullptr;
        const float position = strtof(valueText, &endPointer);
        if (endPointer == valueText) {
            return;
        }
        store->noteFloat(key, position, entry->historyEpsilon, HistoryValueFloat);
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
    if (!SettingsManager::isValidHistoryEntrySize(entrySize)) {
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
    maxStoreDays = SettingsManager::clampHistoryMaxStoreDays(maxDays);
    sweepIntervalMin = SettingsManager::clampHistorySweepIntervalMin(sweepMinutes);
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
    releasePage((uint16_t)records[oldest].pageSlot);
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
    if (!clockIsSane(time(nullptr))) {
        return false;
    }
    SeriesRuntime *runtime = ensureSeries(key, valueType, sizeof(float), epsilon);
    if (runtime == nullptr) {
        return false;
    }
    if (runtime->hasLast && !floatChangedBeyond(runtime->lastFloat, value, epsilon)) {
        return false;
    }
    const uint32_t epoch2010 = unixToEpoch2010(time(nullptr));
    if (!appendRecord(runtime, epoch2010, &value)) {
        return false;
    }
    runtime->hasLast = true;
    runtime->lastFloat = value;
    return true;
}

bool HistoryStore::noteInt(const HistorySeriesKey &key, int32_t value, float epsilon) {
    if (!clockIsSane(time(nullptr))) {
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
    const uint32_t epoch2010 = unixToEpoch2010(time(nullptr));
    if (!appendRecord(runtime, epoch2010, &value)) {
        return false;
    }
    runtime->hasLast = true;
    runtime->lastInt = value;
    return true;
}

bool HistoryStore::noteBool(const HistorySeriesKey &key, bool value) {
    if (!clockIsSane(time(nullptr))) {
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
    const uint32_t epoch2010 = unixToEpoch2010(time(nullptr));
    if (!appendRecord(runtime, epoch2010, &byteValue)) {
        return false;
    }
    runtime->hasLast = true;
    runtime->lastBool = value;
    return true;
}

bool HistoryStore::noteString(const HistorySeriesKey &key, const char *value) {
    if (value == nullptr || !clockIsSane(time(nullptr))) {
        return false;
    }
    SeriesRuntime *runtime =
        ensureSeries(key, HistoryValueString, (uint8_t)kReportableValueTextMaxChars, 0.0f);
    if (runtime == nullptr) {
        return false;
    }
    if (runtime->hasLast && strncmp(runtime->lastString, value, kReportableValueTextMaxChars) == 0) {
        return false;
    }
    char packed[kReportableValueTextMaxChars];
    memset(packed, 0, sizeof(packed));
    strncpy(packed, value, sizeof(packed));
    const uint32_t epoch2010 = unixToEpoch2010(time(nullptr));
    if (!appendRecord(runtime, epoch2010, packed)) {
        return false;
    }
    runtime->hasLast = true;
    strncpy(runtime->lastString, packed, sizeof(runtime->lastString) - 1);
    runtime->lastString[sizeof(runtime->lastString) - 1] = '\0';
    return true;
}

void HistoryStore::syncSeriesFromDevices(DeviceTopicMap *deviceMap) {
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
                const bool isCovering = entry->zigbeeType == ZigbeeDeviceTypeWindowCovering;
                ensureSeries(
                    key,
                    isCovering ? HistoryValueFloat : HistoryValueBool,
                    isCovering ? (uint8_t)sizeof(float) : (uint8_t)1,
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
}

void HistoryStore::loop() {
    if (!available()) {
        return;
    }
    const unsigned long intervalMs = (unsigned long)sweepIntervalMin * 60000UL;
    if (millis() - lastSweepMs < intervalMs) {
        return;
    }
    lastSweepMs = millis();
    sweepAll();
}

void HistoryStore::sweepAll() {
    const time_t nowUnix = time(nullptr);
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
    result.valuesStored = valuesStored;
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
