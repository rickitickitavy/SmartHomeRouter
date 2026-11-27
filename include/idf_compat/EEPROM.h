#pragma once

#include <stdio.h>
#include <string.h>
#include <stdint.h>

class EepromClass {
public:
    bool begin(size_t size) {
        if (size == 0) {
            size = 4096;
        }
        if (size > sizeof(storage)) {
            size = sizeof(storage);
        }
        length = size;
        FILE *file = fopen("/eeprom.bin", "rb");
        if (file != nullptr) {
            fread(storage, 1, length, file);
            fclose(file);
        } else {
            memset(storage, 0xFF, length);
        }
        dirty = false;
        return true;
    }

    uint8_t read(int address) const {
        if (address < 0 || (size_t)address >= length) {
            return 0xFF;
        }
        return storage[address];
    }

    void write(int address, uint8_t value) {
        if (address < 0 || (size_t)address >= length) {
            return;
        }
        if (storage[address] != value) {
            storage[address] = value;
            dirty = true;
        }
    }

    bool commit() {
        if (!dirty) {
            return true;
        }
        FILE *file = fopen("/eeprom.bin", "wb");
        if (file == nullptr) {
            return false;
        }
        const size_t written = fwrite(storage, 1, length, file);
        fclose(file);
        dirty = written != length;
        return !dirty;
    }

    void end() {}

private:
    uint8_t storage[4096];
    size_t length = 4096;
    bool dirty = false;
};

inline EepromClass EEPROM;
