#include "StoreZip.h"

#include <stdlib.h>
#include <string.h>
#include <esp_heap_caps.h>

namespace {

void writeU16(uint8_t *dest, uint16_t value) {
    dest[0] = (uint8_t)(value & 0xffu);
    dest[1] = (uint8_t)((value >> 8) & 0xffu);
}

void writeU32(uint8_t *dest, uint32_t value) {
    dest[0] = (uint8_t)(value & 0xffu);
    dest[1] = (uint8_t)((value >> 8) & 0xffu);
    dest[2] = (uint8_t)((value >> 16) & 0xffu);
    dest[3] = (uint8_t)((value >> 24) & 0xffu);
}

uint16_t readU16(const uint8_t *src) {
    return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
}

uint32_t readU32(const uint8_t *src) {
    return (uint32_t)src[0] | ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16)
        | ((uint32_t)src[3] << 24);
}

uint8_t *allocPsram(size_t bytes) {
    uint8_t *block = (uint8_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (block == nullptr) {
        block = (uint8_t *)malloc(bytes);
    }
    return block;
}

}  // namespace

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
    uint32_t crc = 0xffffffffu;
    for (size_t index = 0; index < length; index++) {
        crc ^= data[index];
        for (int bit = 0; bit < 8; bit++) {
            const uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xedb88320u & mask);
        }
    }
    return ~crc;
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
    size_t offset = 0;
    while (offset + 30 <= length) {
        const uint32_t signature = readU32(bytes + offset);
        if (signature != 0x04034b50u) {
            break;
        }
        const uint16_t method = readU16(bytes + offset + 8);
        const uint32_t compSize = readU32(bytes + offset + 18);
        const uint16_t nameLen = readU16(bytes + offset + 26);
        const uint16_t extraLen = readU16(bytes + offset + 28);
        if (method != 0) {
            clearEntries();
            return false;
        }
        if (offset + 30u + nameLen + extraLen + compSize > length) {
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
        memcpy(pathCopy, bytes + offset + 30, nameLen);
        pathCopy[nameLen] = '\0';
        entry->path = pathCopy;
        entry->data = bytes + offset + 30 + nameLen + extraLen;
        entry->length = compSize;
        entry->next = head;
        head = entry;
        offset += 30u + nameLen + extraLen + compSize;
    }
    return head != nullptr || length >= 22;
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
