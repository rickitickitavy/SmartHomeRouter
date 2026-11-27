#pragma once

#include "FS.h"

#ifndef SDMMC_FREQ_DEFAULT
#define SDMMC_FREQ_DEFAULT 20000
#endif

// IDF host stub: scene bake / history SD paths compile without onboard card I/O.
class SdMmcFS : public FS {
public:
    using FS::remove;

    bool begin(
        const char * /*mountpoint*/ = "/sdcard",
        bool /*mode1bit*/ = false,
        bool /*formatIfFailed*/ = false,
        int /*frequency*/ = SDMMC_FREQ_DEFAULT
    ) {
        return false;
    }

    void end() {}

    bool setPins(int, int, int, int = -1, int = -1, int = -1) {
        return true;
    }

    uint64_t totalBytes() {
        return 0;
    }

    uint64_t usedBytes() {
        return 0;
    }

    File open(const char * /*path*/, const char * /*mode*/ = "r") override {
        return File();
    }

    bool exists(const char * /*path*/) override {
        return false;
    }

    bool remove(const char * /*path*/) override {
        return false;
    }

    bool mkdir(const char * /*path*/) override {
        return false;
    }

    bool rename(const char * /*pathFrom*/, const char * /*pathTo*/) override {
        return false;
    }
};

static SdMmcFS SD_MMC;
