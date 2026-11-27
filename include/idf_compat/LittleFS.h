#pragma once

#include "FS.h"

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

class LittleFSFS : public FS {
public:
    using FS::remove;

    bool begin(bool /*formatOnFail*/ = false) {
        return true;
    }

    void end() {}

    File open(const char *path, const char *mode = "r") override {
        if (path == nullptr || mode == nullptr) {
            return File();
        }
        FILE *handle = fopen(path, mode);
        return File(handle, path, false);
    }

    bool exists(const char *path) override {
        if (path == nullptr) {
            return false;
        }
        struct stat info = {};
        return stat(path, &info) == 0;
    }

    bool remove(const char *path) override {
        if (path == nullptr) {
            return false;
        }
        return unlink(path) == 0 || errno == ENOENT;
    }

    bool mkdir(const char *path) override {
        if (path == nullptr) {
            return false;
        }
        if (::mkdir(path, 0777) == 0) {
            return true;
        }
        return errno == EEXIST;
    }

    bool rename(const char *pathFrom, const char *pathTo) override {
        if (pathFrom == nullptr || pathTo == nullptr) {
            return false;
        }
        return ::rename(pathFrom, pathTo) == 0;
    }

    size_t totalBytes() {
        return 0;
    }

    size_t usedBytes() {
        return 0;
    }
};

static LittleFSFS LittleFS;
