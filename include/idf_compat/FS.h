#pragma once

#include "Arduino.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

class File : public Stream {
public:
    File() : handle(nullptr), cachedSize(0), isDirectoryFlag(false) {
        pathStored[0] = '\0';
    }

    explicit File(FILE *fileHandle, const char *path = nullptr, bool directory = false)
        : handle(fileHandle), cachedSize(0), isDirectoryFlag(directory) {
        pathStored[0] = '\0';
        if (path != nullptr) {
            strncpy(pathStored, path, sizeof(pathStored) - 1);
        }
        if (handle != nullptr && !directory) {
            long pos = ftell(handle);
            if (fseek(handle, 0, SEEK_END) == 0) {
                long end = ftell(handle);
                if (end >= 0) {
                    cachedSize = (size_t)end;
                }
            }
            if (pos >= 0) {
                fseek(handle, pos, SEEK_SET);
            }
        }
    }

    File(const File &) = delete;
    File &operator=(const File &) = delete;

    File(File &&other) noexcept
        : handle(other.handle), cachedSize(other.cachedSize), isDirectoryFlag(other.isDirectoryFlag) {
        memcpy(pathStored, other.pathStored, sizeof(pathStored));
        other.handle = nullptr;
        other.cachedSize = 0;
        other.isDirectoryFlag = false;
        other.pathStored[0] = '\0';
    }

    File &operator=(File &&other) noexcept {
        if (this != &other) {
            close();
            handle = other.handle;
            cachedSize = other.cachedSize;
            isDirectoryFlag = other.isDirectoryFlag;
            memcpy(pathStored, other.pathStored, sizeof(pathStored));
            other.handle = nullptr;
            other.cachedSize = 0;
            other.isDirectoryFlag = false;
            other.pathStored[0] = '\0';
        }
        return *this;
    }

    ~File() {
        close();
    }

    operator bool() const {
        return handle != nullptr || isDirectoryFlag;
    }

    void close() {
        if (handle != nullptr) {
            fclose(handle);
            handle = nullptr;
        }
    }

    size_t size() const {
        return cachedSize;
    }

    const char *name() const {
        return pathStored;
    }

    bool isDirectory() const {
        return isDirectoryFlag;
    }

    int available() override {
        if (handle == nullptr) {
            return 0;
        }
        long pos = ftell(handle);
        if (pos < 0) {
            return 0;
        }
        if ((size_t)pos >= cachedSize) {
            return 0;
        }
        return (int)(cachedSize - (size_t)pos);
    }

    int read() override {
        if (handle == nullptr) {
            return -1;
        }
        return fgetc(handle);
    }

    int peek() override {
        if (handle == nullptr) {
            return -1;
        }
        const int value = fgetc(handle);
        if (value >= 0) {
            ungetc(value, handle);
        }
        return value;
    }

    size_t readBytes(char *buffer, size_t length) {
        if (handle == nullptr || buffer == nullptr || length == 0) {
            return 0;
        }
        return fread(buffer, 1, length, handle);
    }

    size_t read(uint8_t *buffer, size_t length) {
        return readBytes(reinterpret_cast<char *>(buffer), length);
    }

    String readString() {
        String out;
        if (handle == nullptr) {
            return out;
        }
        char chunk[256];
        while (true) {
            const size_t got = fread(chunk, 1, sizeof(chunk), handle);
            if (got == 0) {
                break;
            }
            out.concat(chunk, (unsigned int)got);
        }
        return out;
    }

    size_t write(uint8_t character) override {
        if (handle == nullptr) {
            return 0;
        }
        return fputc(character, handle) == EOF ? 0 : 1;
    }

    size_t write(const uint8_t *buffer, size_t size) override {
        if (handle == nullptr || buffer == nullptr || size == 0) {
            return 0;
        }
        return fwrite(buffer, 1, size, handle);
    }

    bool seek(uint32_t position) {
        if (handle == nullptr) {
            return false;
        }
        return fseek(handle, (long)position, SEEK_SET) == 0;
    }

    bool seek(uint32_t position, int mode) {
        if (handle == nullptr) {
            return false;
        }
        int whence = SEEK_SET;
        if (mode == 1) {
            whence = SEEK_CUR;
        } else if (mode == 2) {
            whence = SEEK_END;
        }
        return fseek(handle, (long)position, whence) == 0;
    }

    File openNextFile(const char * /*mode*/ = "r") {
        return File();
    }

    void rewindDirectory() {}

private:
    FILE *handle;
    char pathStored[192];
    size_t cachedSize;
    bool isDirectoryFlag;
};

class FS {
public:
    virtual ~FS() = default;
    virtual File open(const char *path, const char *mode = "r") = 0;
    virtual bool exists(const char *path) = 0;
    virtual bool remove(const char *path) = 0;
    virtual bool remove(const String &path) {
        return remove(path.c_str());
    }
    virtual bool mkdir(const char *path) = 0;
    virtual bool rename(const char *pathFrom, const char *pathTo) = 0;
};
