#pragma once

#include <stddef.h>
#include <stdint.h>

// Uncompressed (store-method) ZIP writer/reader for full backup archives.
class StoreZipWriter {
public:
    StoreZipWriter();
    ~StoreZipWriter();

    bool addFile(const char *path, const uint8_t *data, size_t length);
    // Allocates PSRAM buffer; caller frees with heap_caps_free / free.
    bool finish(uint8_t **outBytes, size_t *outLength);

private:
    struct Entry {
        char *path = nullptr;
        uint8_t *data = nullptr;
        size_t length = 0;
        uint32_t crc = 0;
        Entry *next = nullptr;
    };

    Entry *head = nullptr;
    Entry *tail = nullptr;
    size_t entryCount = 0;
    size_t totalDataBytes = 0;

    void clearEntries();
    static uint32_t crc32(const uint8_t *data, size_t length);
};

class StoreZipReader {
public:
    StoreZipReader();
    ~StoreZipReader();
    bool open(const uint8_t *bytes, size_t length);
    bool findFile(const char *path, const uint8_t **dataOut, size_t *lengthOut) const;
    // Visit each member; return false from visitor to stop.
    void forEach(
        bool (*visitor)(const char *path, const uint8_t *data, size_t length, void *context),
        void *context
    ) const;

private:
    struct Entry {
        const char *path = nullptr;
        const uint8_t *data = nullptr;
        size_t length = 0;
        Entry *next = nullptr;
    };

    const uint8_t *archiveBytes = nullptr;
    size_t archiveLength = 0;
    Entry *head = nullptr;

    void clearEntries();
};
