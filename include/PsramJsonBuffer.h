#pragma once

#include <Arduino.h>
#include <FS.h>
#include <stdlib.h>
#include <string.h>

#include <esp_heap_caps.h>

#if defined(BOARD_HAS_PSRAM)
#include <esp32-hal-psram.h>
#endif

// Growable JSON text buffer. Prefers PSRAM on the host so large load/save/list
// documents do not exhaust internal heap. Falls back to internal malloc when
// PSRAM is absent or allocation fails.
class PsramJsonBuffer {
public:
    PsramJsonBuffer() = default;
    ~PsramJsonBuffer() {
        release();
    }

    PsramJsonBuffer(const PsramJsonBuffer &) = delete;
    PsramJsonBuffer &operator=(const PsramJsonBuffer &) = delete;

    PsramJsonBuffer(PsramJsonBuffer &&other) noexcept {
        moveFrom(other);
    }

    PsramJsonBuffer &operator=(PsramJsonBuffer &&other) noexcept {
        if (this != &other) {
            release();
            moveFrom(other);
        }
        return *this;
    }

    bool ok() const {
        return !failed;
    }

    size_t length() const {
        return used;
    }

    const char *c_str() const {
        return bytes != nullptr ? bytes : "";
    }

    char charAt(unsigned index) const {
        if (bytes == nullptr || index >= used) {
            return '\0';
        }
        return bytes[index];
    }

    void clear() {
        used = 0;
        failed = false;
        if (bytes != nullptr) {
            bytes[0] = '\0';
        }
    }

    bool reserve(size_t minCapacity) {
        if (failed) {
            return false;
        }
        if (minCapacity < 1) {
            minCapacity = 1;
        }
        if (capacity >= minCapacity) {
            return true;
        }
        size_t newCapacity = capacity > 0 ? capacity : 256;
        while (newCapacity < minCapacity) {
            if (newCapacity > (SIZE_MAX / 2)) {
                newCapacity = minCapacity;
                break;
            }
            newCapacity *= 2;
        }
        char *grown = (char *)allocate(newCapacity);
        if (grown == nullptr) {
            failed = true;
            return false;
        }
        if (bytes != nullptr && used > 0) {
            memcpy(grown, bytes, used);
        }
        grown[used] = '\0';
        releaseBytesOnly();
        bytes = grown;
        capacity = newCapacity;
        return true;
    }

    bool append(const char *text) {
        if (failed) {
            return false;
        }
        if (text == nullptr || text[0] == '\0') {
            return true;
        }
        const size_t textLength = strlen(text);
        if (!reserve(used + textLength + 1)) {
            return false;
        }
        memcpy(bytes + used, text, textLength);
        used += textLength;
        bytes[used] = '\0';
        return true;
    }

    bool append(char character) {
        if (failed) {
            return false;
        }
        if (!reserve(used + 2)) {
            return false;
        }
        bytes[used++] = character;
        bytes[used] = '\0';
        return true;
    }

    bool append(const String &text) {
        return append(text.c_str());
    }

    bool append(const __FlashStringHelper *text) {
        if (failed) {
            return false;
        }
        if (text == nullptr) {
            return true;
        }
        return append(String(text));
    }

    PsramJsonBuffer &operator+=(const char *text) {
        append(text);
        return *this;
    }

    PsramJsonBuffer &operator+=(char character) {
        append(character);
        return *this;
    }

    PsramJsonBuffer &operator+=(const String &text) {
        append(text);
        return *this;
    }

    PsramJsonBuffer &operator+=(const __FlashStringHelper *text) {
        append(text);
        return *this;
    }

    void trim() {
        if (bytes == nullptr || used == 0) {
            return;
        }
        size_t start = 0;
        while (start < used
            && (bytes[start] == ' '
                || bytes[start] == '\t'
                || bytes[start] == '\r'
                || bytes[start] == '\n')) {
            start++;
        }
        size_t end = used;
        while (end > start
            && (bytes[end - 1] == ' '
                || bytes[end - 1] == '\t'
                || bytes[end - 1] == '\r'
                || bytes[end - 1] == '\n')) {
            end--;
        }
        if (start == 0 && end == used) {
            return;
        }
        const size_t newLength = end - start;
        if (start > 0 && newLength > 0) {
            memmove(bytes, bytes + start, newLength);
        }
        used = newLength;
        bytes[used] = '\0';
    }

    bool assignSlice(const char *start, const char *endInclusive) {
        clear();
        if (start == nullptr || endInclusive == nullptr || endInclusive < start) {
            failed = true;
            return false;
        }
        const size_t sliceLength = (size_t)(endInclusive - start + 1);
        if (!reserve(sliceLength + 1)) {
            return false;
        }
        memcpy(bytes, start, sliceLength);
        used = sliceLength;
        bytes[used] = '\0';
        return true;
    }

    bool assignFromFile(File &file) {
        clear();
        if (!file) {
            failed = true;
            return false;
        }
        const size_t fileSize = file.size();
        if (!reserve(fileSize + 1)) {
            return false;
        }
        size_t totalRead = 0;
        while (totalRead < fileSize) {
            const size_t chunk = file.readBytes(bytes + totalRead, fileSize - totalRead);
            if (chunk == 0) {
                break;
            }
            totalRead += chunk;
        }
        used = totalRead;
        bytes[used] = '\0';
        return true;
    }

    bool writeToFile(File &file) const {
        if (!file || failed) {
            return false;
        }
        if (used == 0) {
            return true;
        }
        const size_t written = file.write((const uint8_t *)bytes, used);
        return written == used;
    }

    // Bridge for APIs that still return Arduino String. One allocation.
    String toString() const {
        if (failed) {
            return String();
        }
        if (bytes == nullptr || used == 0) {
            return String("");
        }
        String out;
        if (!out.reserve((unsigned int)used)) {
            return String();
        }
        if (!out.concat(bytes, (unsigned int)used)) {
            return String();
        }
        return out;
    }

private:
    char *bytes = nullptr;
    size_t used = 0;
    size_t capacity = 0;
    bool failed = false;

    static void *allocate(size_t byteCount) {
        if (byteCount == 0) {
            return nullptr;
        }
#if defined(BOARD_HAS_PSRAM)
        void *block = heap_caps_malloc(byteCount, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (block == nullptr) {
            block = ps_malloc(byteCount);
        }
        if (block != nullptr) {
            return block;
        }
#endif
        return malloc(byteCount);
    }

    void releaseBytesOnly() {
        if (bytes != nullptr) {
            free(bytes);
            bytes = nullptr;
        }
        capacity = 0;
    }

    void release() {
        releaseBytesOnly();
        used = 0;
        failed = false;
    }

    void moveFrom(PsramJsonBuffer &other) {
        bytes = other.bytes;
        used = other.used;
        capacity = other.capacity;
        failed = other.failed;
        other.bytes = nullptr;
        other.used = 0;
        other.capacity = 0;
        other.failed = false;
    }
};
