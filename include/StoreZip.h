#pragma once

#include <stddef.h>
#include <stdint.h>

// Uncompressed (store-method) ZIP writer that buffers every member in DRAM.
// Prefer StoreZipStreamEncoder for large archives (Full backup).
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

// Streaming store-method ZIP encoder: local headers use GP bit 3 + data descriptors.
// Retains only per-member metadata (path/crc/size/offset) — never the finished archive.
class StoreZipStreamEncoder {
public:
    StoreZipStreamEncoder();
    ~StoreZipStreamEncoder();

    bool startFile(const char *path);
    bool finishFile();
    bool finishArchive();

    // Emit queued header / descriptor / central-directory bytes into out.
    // Sets *written; returns true when the current pending block is fully emitted.
    bool emitPending(uint8_t *out, size_t outMax, size_t *written);

    // Copy member payload into out and update CRC/size. Returns bytes taken from data.
    size_t writeFileData(
        const uint8_t *data,
        size_t dataLen,
        uint8_t *out,
        size_t outMax,
        size_t *written
    );
    // Note payload already placed in the HTTP/out buffer (no extra copy).
    void noteFileBytes(const uint8_t *data, size_t length);

    bool hasPending() const { return pendingLen > pendingOff; }
    bool archiveFinished() const { return archiveDone && !hasPending(); }
    bool failed() const { return failFlag; }
    uint32_t bytesEmitted() const { return (uint32_t)streamOffset; }

    static uint32_t crc32Update(uint32_t crc, const uint8_t *data, size_t length);
    static uint32_t crc32Final(uint32_t crc);

private:
    struct Meta {
        char *path = nullptr;
        uint32_t crc = 0;
        uint32_t size = 0;
        uint32_t localHeaderOffset = 0;
        Meta *next = nullptr;
    };

    enum PendingKind {
        PendingNone = 0,
        PendingLocalHeader,
        PendingDataDesc,
        PendingCentral,
        PendingEocd
    };

    Meta *metaHead = nullptr;
    Meta *metaTail = nullptr;
    Meta *centralEmit = nullptr;
    size_t entryCount = 0;
    size_t streamOffset = 0;

    bool fileOpen = false;
    bool archiveDone = false;
    bool failFlag = false;
    uint32_t runningCrc = 0xffffffffu;
    uint32_t runningSize = 0;
    uint32_t currentLocalOffset = 0;
    char currentPath[96];

    PendingKind pendingKind = PendingNone;
    uint8_t pendingBuf[160];
    size_t pendingLen = 0;
    size_t pendingOff = 0;
    size_t centralBytesTotal = 0;
    size_t centralBytesEmitted = 0;
    size_t localBytesTotal = 0;

    void clearMeta();
    void setFailed();
    bool queueBytes(const uint8_t *bytes, size_t length);
    bool queueLocalHeader(const char *path);
    bool queueDataDescriptor(uint32_t crc, uint32_t size);
    bool queueNextCentralOrEocd();
    static void writeU16(uint8_t *dest, uint16_t value);
    static void writeU32(uint8_t *dest, uint32_t value);
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
    static uint16_t readU16(const uint8_t *src);
    static uint32_t readU32(const uint8_t *src);
};
