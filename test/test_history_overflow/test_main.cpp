#include <unity.h>

#include "HistoryStore.h"

#include <string.h>

namespace {

constexpr time_t kBaseUnix = 1735689600; // 2025-01-01 UTC (sane)
constexpr uint16_t kEntrySize = 256; // (256-12)/12 = 20 pages max per series
constexpr uint16_t kMaxPagesPerEntry = (kEntrySize - HISTORY_ENTRY_HEADER_BYTES) / HISTORY_PAGE_RECORD_BYTES;
// float sample: 4 epoch + 4 value; page header 4 bytes → 511 samples / page
constexpr uint16_t kFloatSamplesPerPage = (HISTORY_PAGE_BYTES - 4) / 8;

HistorySeriesKey makeKey() {
    HistorySeriesKey key{};
    memset(key.ieee, 0xA5, sizeof(key.ieee));
    key.kind = HistorySeriesMainState;
    key.channel = 1;
    key.cluster = 0;
    key.attribute = 0;
    return key;
}

void fillPages(HistoryStore &store, const HistorySeriesKey &key, uint16_t pageTarget, float *valueOut) {
    float value = *valueOut;
    const uint32_t samplesNeeded = (uint32_t)pageTarget * (uint32_t)kFloatSamplesPerPage;
    for (uint32_t index = 0; index < samplesNeeded; index++) {
        value += 1.0f;
        HistoryStore::setTestUnixNow(kBaseUnix + (time_t)index);
        TEST_ASSERT_TRUE_MESSAGE(
            store.noteFloat(key, value, 0.0f, HistoryValueFloat),
            "noteFloat failed while filling pages"
        );
    }
    *valueOut = value;
}

} // namespace

void setUp() {
    HistoryStore::clearTestUnixNow();
}

void tearDown() {
    HistoryStore::clearTestUnixNow();
}

void test_max_pages_for_entry_size_256() {
    TEST_ASSERT_EQUAL_UINT16(20, HistoryStore::maxPagesForEntrySize(256));
    TEST_ASSERT_EQUAL_UINT16(kMaxPagesPerEntry, HistoryStore::maxPagesForEntrySize(kEntrySize));
}

void test_overflow_purges_oldest_and_wraps_tail_past_head() {
    HistoryStore store;
    TEST_ASSERT_TRUE(store.begin(256, kEntrySize)); // 64 slab pages
    store.setRetention(20, 10);

    const HistorySeriesKey key = makeKey();
    float value = 0.0f;

    // Fill exactly max pages (oldest=0, newest=max-1, no wrap yet).
    fillPages(store, key, kMaxPagesPerEntry, &value);

    uint16_t oldest = 0;
    uint16_t newest = 0;
    uint16_t pageCount = 0;
    TEST_ASSERT_TRUE(store.seriesRingState(key, &oldest, &newest, &pageCount));
    TEST_ASSERT_EQUAL_UINT16(kMaxPagesPerEntry, pageCount);
    TEST_ASSERT_EQUAL_UINT16(0, oldest);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)(kMaxPagesPerEntry - 1), newest);
    TEST_ASSERT_TRUE(oldest <= newest);

    // One more page forces purge of oldest and wrap of newest to 0.
    // Tail (oldest) then sits past Head (newest): oldest > newest.
    fillPages(store, key, 1, &value);
    TEST_ASSERT_TRUE(store.seriesRingState(key, &oldest, &newest, &pageCount));
    TEST_ASSERT_EQUAL_UINT16(kMaxPagesPerEntry, pageCount);
    TEST_ASSERT_EQUAL_UINT16(0, newest);
    TEST_ASSERT_TRUE_MESSAGE(oldest > newest, "expected wrapped ring: oldestIndex > newestIndex");
    TEST_ASSERT_EQUAL_UINT16(1, oldest);

    // Keep overflowing: page count stays capped; ring stays wrapped.
    fillPages(store, key, 3, &value);
    TEST_ASSERT_TRUE(store.seriesRingState(key, &oldest, &newest, &pageCount));
    TEST_ASSERT_EQUAL_UINT16(kMaxPagesPerEntry, pageCount);
    TEST_ASSERT_TRUE(oldest > newest);

    HistoryStoreStats stats = store.stats();
    TEST_ASSERT_EQUAL_UINT16(1, stats.entriesCount);
    TEST_ASSERT_EQUAL_UINT16(kMaxPagesPerEntry, stats.pagesUsed);

    store.end();
}

void test_global_outdated_purge_drops_old_pages() {
    HistoryStore store;
    TEST_ASSERT_TRUE(store.begin(128, kEntrySize));
    store.setRetention(1, 1); // 1 day retention

    const HistorySeriesKey key = makeKey();
    float value = 0.0f;

    // Seed a few pages at day 0.
    HistoryStore::setTestUnixNow(kBaseUnix);
    fillPages(store, key, 3, &value);

    uint16_t oldest = 0;
    uint16_t newest = 0;
    uint16_t pageCountBefore = 0;
    TEST_ASSERT_TRUE(store.seriesRingState(key, &oldest, &newest, &pageCountBefore));
    TEST_ASSERT_EQUAL_UINT16(3, pageCountBefore);

    // Jump > 1 day past the oldest page lastDate and run global sweep.
    HistoryStore::setTestUnixNow(kBaseUnix + (2 * 86400));
    store.forceSweepForTest();

    uint16_t pageCountAfter = 0;
    TEST_ASSERT_TRUE(store.seriesRingState(key, &oldest, &newest, &pageCountAfter));
    TEST_ASSERT_TRUE_MESSAGE(
        pageCountAfter < pageCountBefore,
        "outdated global sweep should purge old pages"
    );
    TEST_ASSERT_EQUAL_UINT16(0, pageCountAfter);

    store.end();
}

void test_slab_overflow_reclaims_by_purging_oldest() {
    // Only a few slab pages total — smaller than entry capacity — so acquirePage
    // fails and append must purgeOldest until a free slab slot appears.
    HistoryStore store;
    TEST_ASSERT_TRUE(store.begin(16, kEntrySize)); // 4 slab pages
    store.setRetention(20, 10);

    const HistorySeriesKey key = makeKey();
    float value = 0.0f;
    fillPages(store, key, 4, &value);

    uint16_t oldest = 0;
    uint16_t newest = 0;
    uint16_t pageCount = 0;
    TEST_ASSERT_TRUE(store.seriesRingState(key, &oldest, &newest, &pageCount));
    TEST_ASSERT_EQUAL_UINT16(4, pageCount);

    const HistoryStoreStats before = store.stats();
    TEST_ASSERT_EQUAL_UINT16(4, before.pagesUsed);

    fillPages(store, key, 1, &value);
    TEST_ASSERT_TRUE(store.seriesRingState(key, &oldest, &newest, &pageCount));
    TEST_ASSERT_EQUAL_UINT16(4, pageCount);
    TEST_ASSERT_TRUE(oldest > 0 || newest < 3);

    const HistoryStoreStats after = store.stats();
    TEST_ASSERT_EQUAL_UINT16(4, after.pagesUsed);
    TEST_ASSERT_TRUE(after.valuesStored > before.valuesStored);

    store.end();
}

void test_snapshot_round_trip_and_shrink_entry_size() {
    HistoryStore store;
    TEST_ASSERT_TRUE(store.begin(256, 1024));
    store.setRetention(20, 10);
    const HistorySeriesKey key = makeKey();
    float value = 0.0f;
    fillPages(store, key, 8, &value);

    uint8_t *bytes = nullptr;
    size_t len = 0;
    TEST_ASSERT_TRUE(store.exportSnapshotToHeap(&bytes, &len));
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_TRUE(len > 0);
    store.end();

    HistoryStore smaller;
    TEST_ASSERT_TRUE(smaller.begin(256, 256)); // fewer page records per entry
    TEST_ASSERT_TRUE(smaller.importSnapshotFromHeap(bytes, len));
    free(bytes);

    uint16_t oldest = 0;
    uint16_t newest = 0;
    uint16_t pageCount = 0;
    TEST_ASSERT_TRUE(smaller.seriesRingState(key, &oldest, &newest, &pageCount));
    TEST_ASSERT_TRUE(pageCount > 0);
    TEST_ASSERT_TRUE(pageCount <= HistoryStore::maxPagesForEntrySize(256));
    smaller.end();
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_max_pages_for_entry_size_256);
    RUN_TEST(test_overflow_purges_oldest_and_wraps_tail_past_head);
    RUN_TEST(test_global_outdated_purge_drops_old_pages);
    RUN_TEST(test_slab_overflow_reclaims_by_purging_oldest);
    RUN_TEST(test_snapshot_round_trip_and_shrink_entry_size);
    return UNITY_END();
}
