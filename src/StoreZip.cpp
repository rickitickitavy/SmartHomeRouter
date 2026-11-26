#include "StoreZip.h"

#include <stdlib.h>
#include <string.h>
#include <esp_heap_caps.h>

namespace {

uint8_t *allocPsram(size_t bytes) {
    uint8_t *block = (uint8_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (block == nullptr) {
        block = (uint8_t *)malloc(bytes);
    }
    return block;
}

}  // namespace

void StoreZipStreamEncoder::writeU16(uint8_t *dest, uint16_t value) {
    dest[0] = (uint8_t)(value & 0xffu);
    dest[1] = (uint8_t)((value >> 8) & 0xffu);
}

void StoreZipStreamEncoder::writeU32(uint8_t *dest, uint32_t value) {
    dest[0] = (uint8_t)(value & 0xffu);
    dest[1] = (uint8_t)((value >> 8) & 0xffu);
    dest[2] = (uint8_t)((value >> 16) & 0xffu);
    dest[3] = (uint8_t)((value >> 24) & 0xffu);
}

uint32_t StoreZipStreamEncoder::crc32Update(uint32_t crc, const uint8_t *data, size_t length) {
    if (data == nullptr && length > 0) {
        return crc;
    }
    for (size_t index = 0; index < length; index++) {
        crc ^= data[index];
        for (int bit = 0; bit < 8; bit++) {
            const uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xedb88320u & mask);
        }
    }
    return crc;
}

uint32_t StoreZipStreamEncoder::crc32Final(uint32_t crc) {
    return ~crc;
}

StoreZipStreamEncoder::StoreZipStreamEncoder() {
    currentPath[0] = '\0';
}

StoreZipStreamEncoder::~StoreZipStreamEncoder() {
    clearMeta();
}

void StoreZipStreamEncoder::clearMeta() {
    Meta *entry = metaHead;
    while (entry != nullptr) {
        Meta *next = entry->next;
        free(entry->path);
        free(entry);
        entry = next;
    }
    metaHead = nullptr;
    metaTail = nullptr;
    centralEmit = nullptr;
    entryCount = 0;
}

void StoreZipStreamEncoder::setFailed() {
    failFlag = true;
    pendingKind = PendingNone;
    pendingLen = 0;
    pendingOff = 0;
}

bool StoreZipStreamEncoder::queueBytes(const uint8_t *bytes, size_t length) {
    if (bytes == nullptr || length == 0 || length > sizeof(pendingBuf)) {
        setFailed();
        return false;
    }
    if (hasPending()) {
        setFailed();
        return false;
    }
    memcpy(pendingBuf, bytes, length);
    pendingLen = length;
    pendingOff = 0;
    return true;
}

bool StoreZipStreamEncoder::queueLocalHeader(const char *path) {
    if (path == nullptr || path[0] == '\0') {
        setFailed();
        return false;
    }
    const size_t pathLen = strlen(path);
    if (pathLen == 0 || pathLen >= sizeof(currentPath) || 30u + pathLen > sizeof(pendingBuf)) {
        setFailed();
        return false;
    }
    uint8_t header[160];
    writeU32(header, 0x04034b50u);
    writeU16(header + 4, 20);
    writeU16(header + 6, 0x0008);  // GP bit 3: data descriptor
    writeU16(header + 8, 0);       // store
    writeU16(header + 10, 0);
    writeU16(header + 12, 0);
    writeU32(header + 14, 0);
    writeU32(header + 18, 0);
    writeU32(header + 22, 0);
    writeU16(header + 26, (uint16_t)pathLen);
    writeU16(header + 28, 0);
    memcpy(header + 30, path, pathLen);
    pendingKind = PendingLocalHeader;
    return queueBytes(header, 30u + pathLen);
}

bool StoreZipStreamEncoder::queueDataDescriptor(uint32_t crc, uint32_t size) {
    uint8_t desc[16];
    writeU32(desc, 0x08074b50u);
    writeU32(desc + 4, crc);
    writeU32(desc + 8, size);
    writeU32(desc + 12, size);
    pendingKind = PendingDataDesc;
    return queueBytes(desc, sizeof(desc));
}

bool StoreZipStreamEncoder::startFile(const char *path) {
    if (failFlag || fileOpen || archiveDone || hasPending()) {
        setFailed();
        return false;
    }
    if (path == nullptr || path[0] == '\0' || strlen(path) >= sizeof(currentPath)) {
        setFailed();
        return false;
    }
    memcpy(currentPath, path, strlen(path) + 1);
    currentLocalOffset = (uint32_t)streamOffset;
    runningCrc = 0xffffffffu;
    runningSize = 0;
    fileOpen = true;
    return queueLocalHeader(path);
}

void StoreZipStreamEncoder::noteFileBytes(const uint8_t *data, size_t length) {
    if (!fileOpen || failFlag || length == 0) {
        return;
    }
    runningCrc = crc32Update(runningCrc, data, length);
    runningSize += (uint32_t)length;
    streamOffset += length;
}

size_t StoreZipStreamEncoder::writeFileData(
    const uint8_t *data,
    size_t dataLen,
    uint8_t *out,
    size_t outMax,
    size_t *written
) {
    if (written != nullptr) {
        *written = 0;
    }
    if (!fileOpen || failFlag || hasPending() || data == nullptr || out == nullptr || outMax == 0
        || dataLen == 0) {
        return 0;
    }
    size_t chunk = dataLen;
    if (chunk > outMax) {
        chunk = outMax;
    }
    memcpy(out, data, chunk);
    noteFileBytes(out, chunk);
    if (written != nullptr) {
        *written = chunk;
    }
    return chunk;
}

bool StoreZipStreamEncoder::finishFile() {
    if (!fileOpen || failFlag || hasPending()) {
        setFailed();
        return false;
    }
    const uint32_t crc = crc32Final(runningCrc);
    Meta *entry = (Meta *)calloc(1, sizeof(Meta));
    if (entry == nullptr) {
        setFailed();
        return false;
    }
    const size_t pathLen = strlen(currentPath);
    entry->path = (char *)malloc(pathLen + 1);
    if (entry->path == nullptr) {
        free(entry);
        setFailed();
        return false;
    }
    memcpy(entry->path, currentPath, pathLen + 1);
    entry->crc = crc;
    entry->size = runningSize;
    entry->localHeaderOffset = currentLocalOffset;
    entry->next = nullptr;
    if (metaTail == nullptr) {
        metaHead = entry;
        metaTail = entry;
    } else {
        metaTail->next = entry;
        metaTail = entry;
    }
    entryCount++;
    fileOpen = false;
    return queueDataDescriptor(crc, runningSize);
}

bool StoreZipStreamEncoder::finishArchive() {
    if (failFlag || fileOpen || archiveDone || hasPending()) {
        setFailed();
        return false;
    }
    localBytesTotal = streamOffset;
    centralBytesTotal = 0;
    for (Meta *entry = metaHead; entry != nullptr; entry = entry->next) {
        centralBytesTotal += 46u + strlen(entry->path);
    }
    centralBytesEmitted = 0;
    centralEmit = metaHead;
    archiveDone = true;
    return queueNextCentralOrEocd();
}

bool StoreZipStreamEncoder::queueNextCentralOrEocd() {
    if (centralEmit != nullptr) {
        const size_t pathLen = strlen(centralEmit->path);
        if (46u + pathLen > sizeof(pendingBuf)) {
            setFailed();
            return false;
        }
        uint8_t central[160];
        writeU32(central, 0x02014b50u);
        writeU16(central + 4, 20);
        writeU16(central + 6, 20);
        writeU16(central + 8, 0x0008);
        writeU16(central + 10, 0);
        writeU16(central + 12, 0);
        writeU16(central + 14, 0);
        writeU32(central + 16, centralEmit->crc);
        writeU32(central + 20, centralEmit->size);
        writeU32(central + 24, centralEmit->size);
        writeU16(central + 28, (uint16_t)pathLen);
        writeU16(central + 30, 0);
        writeU16(central + 32, 0);
        writeU16(central + 34, 0);
        writeU16(central + 36, 0);
        writeU32(central + 38, 0);
        writeU32(central + 42, centralEmit->localHeaderOffset);
        memcpy(central + 46, centralEmit->path, pathLen);
        pendingKind = PendingCentral;
        if (!queueBytes(central, 46u + pathLen)) {
            return false;
        }
        centralEmit = centralEmit->next;
        return true;
    }
    uint8_t eocd[22];
    writeU32(eocd, 0x06054b50u);
    writeU16(eocd + 4, 0);
    writeU16(eocd + 6, 0);
    writeU16(eocd + 8, (uint16_t)entryCount);
    writeU16(eocd + 10, (uint16_t)entryCount);
    writeU32(eocd + 12, (uint32_t)centralBytesTotal);
    writeU32(eocd + 16, (uint32_t)localBytesTotal);
    writeU16(eocd + 20, 0);
    pendingKind = PendingEocd;
    return queueBytes(eocd, sizeof(eocd));
}

bool StoreZipStreamEncoder::emitPending(uint8_t *out, size_t outMax, size_t *written) {
    if (written != nullptr) {
        *written = 0;
    }
    if (failFlag || out == nullptr || outMax == 0) {
        return !hasPending();
    }
    size_t totalWritten = 0;
    while (hasPending() && totalWritten < outMax) {
        size_t chunk = pendingLen - pendingOff;
        if (chunk > outMax - totalWritten) {
            chunk = outMax - totalWritten;
        }
        memcpy(out + totalWritten, pendingBuf + pendingOff, chunk);
        pendingOff += chunk;
        streamOffset += chunk;
        totalWritten += chunk;
        if (pendingKind == PendingCentral || pendingKind == PendingEocd) {
            centralBytesEmitted += chunk;
        }
        if (pendingOff < pendingLen) {
            break;
        }
        const PendingKind doneKind = pendingKind;
        pendingKind = PendingNone;
        pendingLen = 0;
        pendingOff = 0;
        if (doneKind == PendingCentral) {
            if (!queueNextCentralOrEocd()) {
                break;
            }
            continue;
        }
        break;
    }
    if (written != nullptr) {
        *written = totalWritten;
    }
    return !hasPending();
}

StoreZipWriter::StoreZipWriter() {}

StoreZipWriter::~StoreZipWriter() {
    clearEntries();
}

void StoreZipWriter::clearEntries() {
    Entry *entry = head;
    while (entry != nullptr) {
        Entry *next = entry->next;
        free(entry->path);
        free(entry->data);
        free(entry);
        entry = next;
    }
    head = nullptr;
    tail = nullptr;
    entryCount = 0;
    totalDataBytes = 0;
}

uint32_t StoreZipWriter::crc32(const uint8_t *data, size_t length) {
    return StoreZipStreamEncoder::crc32Final(
        StoreZipStreamEncoder::crc32Update(0xffffffffu, data, length)
    );
}

bool StoreZipWriter::addFile(const char *path, const uint8_t *data, size_t length) {
    if (path == nullptr || path[0] == '\0' || (length > 0 && data == nullptr)) {
        return false;
    }
    Entry *entry = (Entry *)calloc(1, sizeof(Entry));
    if (entry == nullptr) {
        return false;
    }
    const size_t pathLen = strlen(path);
    entry->path = (char *)malloc(pathLen + 1);
    if (entry->path == nullptr) {
        free(entry);
        return false;
    }
    memcpy(entry->path, path, pathLen + 1);
    if (length > 0) {
        entry->data = (uint8_t *)malloc(length);
        if (entry->data == nullptr) {
            free(entry->path);
            free(entry);
            return false;
        }
        memcpy(entry->data, data, length);
    }
    entry->length = length;
    entry->crc = crc32(data, length);
    entry->next = nullptr;
    if (tail == nullptr) {
        head = entry;
        tail = entry;
    } else {
        tail->next = entry;
        tail = entry;
    }
    entryCount++;
    totalDataBytes += length;
    return true;
}

bool StoreZipWriter::finish(uint8_t **outBytes, size_t *outLength) {
    if (outBytes == nullptr || outLength == nullptr) {
        return false;
    }
    *outBytes = nullptr;
    *outLength = 0;
    size_t localBytes = 0;
    size_t centralBytes = 0;
    for (Entry *entry = head; entry != nullptr; entry = entry->next) {
        const size_t pathLen = strlen(entry->path);
        localBytes += 30u + pathLen + entry->length;
        centralBytes += 46u + pathLen;
    }
    const size_t total = localBytes + centralBytes + 22u;
    uint8_t *buffer = allocPsram(total);
    if (buffer == nullptr) {
        return false;
    }
    auto writeU16 = [](uint8_t *dest, uint16_t value) {
        dest[0] = (uint8_t)(value & 0xffu);
        dest[1] = (uint8_t)((value >> 8) & 0xffu);
    };
    auto writeU32 = [](uint8_t *dest, uint32_t value) {
        dest[0] = (uint8_t)(value & 0xffu);
        dest[1] = (uint8_t)((value >> 8) & 0xffu);
        dest[2] = (uint8_t)((value >> 16) & 0xffu);
        dest[3] = (uint8_t)((value >> 24) & 0xffu);
    };
    size_t offset = 0;
    size_t centralOffset = localBytes;
    for (Entry *entry = head; entry != nullptr; entry = entry->next) {
        const size_t pathLen = strlen(entry->path);
        const uint32_t localHeaderOffset = (uint32_t)offset;
        writeU32(buffer + offset, 0x04034b50u);
        writeU16(buffer + offset + 4, 20);
        writeU16(buffer + offset + 6, 0);
        writeU16(buffer + offset + 8, 0);
        writeU16(buffer + offset + 10, 0);
        writeU16(buffer + offset + 12, 0);
        writeU32(buffer + offset + 14, entry->crc);
        writeU32(buffer + offset + 18, (uint32_t)entry->length);
        writeU32(buffer + offset + 22, (uint32_t)entry->length);
        writeU16(buffer + offset + 26, (uint16_t)pathLen);
        writeU16(buffer + offset + 28, 0);
        memcpy(buffer + offset + 30, entry->path, pathLen);
        if (entry->length > 0) {
            memcpy(buffer + offset + 30 + pathLen, entry->data, entry->length);
        }

        writeU32(buffer + centralOffset, 0x02014b50u);
        writeU16(buffer + centralOffset + 4, 20);
        writeU16(buffer + centralOffset + 6, 20);
        writeU16(buffer + centralOffset + 8, 0);
        writeU16(buffer + centralOffset + 10, 0);
        writeU16(buffer + centralOffset + 12, 0);
        writeU16(buffer + centralOffset + 14, 0);
        writeU32(buffer + centralOffset + 16, entry->crc);
        writeU32(buffer + centralOffset + 20, (uint32_t)entry->length);
        writeU32(buffer + centralOffset + 24, (uint32_t)entry->length);
        writeU16(buffer + centralOffset + 28, (uint16_t)pathLen);
        writeU16(buffer + centralOffset + 30, 0);
        writeU16(buffer + centralOffset + 32, 0);
        writeU16(buffer + centralOffset + 34, 0);
        writeU16(buffer + centralOffset + 36, 0);
        writeU32(buffer + centralOffset + 38, 0);
        writeU32(buffer + centralOffset + 42, localHeaderOffset);
        memcpy(buffer + centralOffset + 46, entry->path, pathLen);

        offset += 30u + pathLen + entry->length;
        centralOffset += 46u + pathLen;
    }
    writeU32(buffer + centralOffset, 0x06054b50u);
    writeU16(buffer + centralOffset + 4, 0);
    writeU16(buffer + centralOffset + 6, 0);
    writeU16(buffer + centralOffset + 8, (uint16_t)entryCount);
    writeU16(buffer + centralOffset + 10, (uint16_t)entryCount);
    writeU32(buffer + centralOffset + 12, (uint32_t)(centralBytes));
    writeU32(buffer + centralOffset + 16, (uint32_t)localBytes);
    writeU16(buffer + centralOffset + 20, 0);
    *outBytes = buffer;
    *outLength = total;
    return true;
}

uint16_t StoreZipReader::readU16(const uint8_t *src) {
    return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
}

uint32_t StoreZipReader::readU32(const uint8_t *src) {
    return (uint32_t)src[0] | ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16)
        | ((uint32_t)src[3] << 24);
}

StoreZipReader::StoreZipReader() {}

StoreZipReader::~StoreZipReader() {
    clearEntries();
}

void StoreZipReader::clearEntries() {
    Entry *entry = head;
    while (entry != nullptr) {
        Entry *next = entry->next;
        free((void *)entry->path);
        free(entry);
        entry = next;
    }
    head = nullptr;
}

bool StoreZipReader::open(const uint8_t *bytes, size_t length) {
    clearEntries();
    archiveBytes = bytes;
    archiveLength = length;
    if (bytes == nullptr || length < 22) {
        return false;
    }

    // Prefer EOCD / central directory so GP bit-3 (data descriptor) ZIPs work.
    size_t eocdOffset = length;
    const size_t scanMax = length < 65557u ? length : 65557u;
    for (size_t back = 22; back <= scanMax; back++) {
        const size_t candidate = length - back;
        if (readU32(bytes + candidate) == 0x06054b50u) {
            eocdOffset = candidate;
            break;
        }
    }
    if (eocdOffset + 22 > length) {
        return false;
    }
    const uint16_t totalEntries = readU16(bytes + eocdOffset + 10);
    const uint32_t centralSize = readU32(bytes + eocdOffset + 12);
    const uint32_t centralOffset = readU32(bytes + eocdOffset + 16);
    if ((size_t)centralOffset + (size_t)centralSize > length) {
        clearEntries();
        return false;
    }

    size_t offset = centralOffset;
    const size_t centralEnd = (size_t)centralOffset + (size_t)centralSize;
    uint16_t parsed = 0;
    while (offset + 46 <= centralEnd && parsed < totalEntries) {
        if (readU32(bytes + offset) != 0x02014b50u) {
            clearEntries();
            return false;
        }
        const uint16_t method = readU16(bytes + offset + 10);
        const uint32_t compSize = readU32(bytes + offset + 20);
        const uint16_t nameLen = readU16(bytes + offset + 28);
        const uint16_t extraLen = readU16(bytes + offset + 30);
        const uint16_t commentLen = readU16(bytes + offset + 32);
        const uint32_t localHeaderOffset = readU32(bytes + offset + 42);
        if (method != 0) {
            clearEntries();
            return false;
        }
        if (offset + 46u + nameLen + extraLen + commentLen > centralEnd) {
            clearEntries();
            return false;
        }
        if ((size_t)localHeaderOffset + 30u > length) {
            clearEntries();
            return false;
        }
        if (readU32(bytes + localHeaderOffset) != 0x04034b50u) {
            clearEntries();
            return false;
        }
        const uint16_t localNameLen = readU16(bytes + localHeaderOffset + 26);
        const uint16_t localExtraLen = readU16(bytes + localHeaderOffset + 28);
        const size_t dataOffset =
            (size_t)localHeaderOffset + 30u + (size_t)localNameLen + (size_t)localExtraLen;
        if (dataOffset + (size_t)compSize > length) {
            clearEntries();
            return false;
        }

        Entry *entry = (Entry *)calloc(1, sizeof(Entry));
        if (entry == nullptr) {
            clearEntries();
            return false;
        }
        char *pathCopy = (char *)malloc((size_t)nameLen + 1u);
        if (pathCopy == nullptr) {
            free(entry);
            clearEntries();
            return false;
        }
        memcpy(pathCopy, bytes + offset + 46, nameLen);
        pathCopy[nameLen] = '\0';
        entry->path = pathCopy;
        entry->data = bytes + dataOffset;
        entry->length = compSize;
        entry->next = head;
        head = entry;

        offset += 46u + nameLen + extraLen + commentLen;
        parsed++;
    }
    return head != nullptr || totalEntries == 0;
}

bool StoreZipReader::findFile(const char *path, const uint8_t **dataOut, size_t *lengthOut) const {
    if (path == nullptr || dataOut == nullptr || lengthOut == nullptr) {
        return false;
    }
    for (Entry *entry = head; entry != nullptr; entry = entry->next) {
        if (strcmp(entry->path, path) == 0) {
            *dataOut = entry->data;
            *lengthOut = entry->length;
            return true;
        }
    }
    return false;
}

void StoreZipReader::forEach(
    bool (*visitor)(const char *path, const uint8_t *data, size_t length, void *context),
    void *context
) const {
    if (visitor == nullptr) {
        return;
    }
    for (Entry *entry = head; entry != nullptr; entry = entry->next) {
        if (!visitor(entry->path, entry->data, entry->length, context)) {
            return;
        }
    }
}
