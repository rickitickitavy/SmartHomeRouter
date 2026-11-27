#pragma once

#include "Arduino.h"
#include "WiFi.h"

class WiFiClient : public Stream {
public:
    bool connect(const char * /*host*/, uint16_t /*port*/, int32_t /*timeout*/ = 0) {
        return false;
    }
    bool connect(IPAddress /*ip*/, uint16_t /*port*/, int32_t /*timeout*/ = 0) {
        return false;
    }
    void stop() {}
    void setTimeout(uint32_t) {}
    bool connected() {
        return false;
    }
    int available() override {
        return 0;
    }
    int read() override {
        return -1;
    }
    int peek() override {
        return -1;
    }
    size_t readBytes(char * /*buffer*/, size_t /*length*/) {
        return 0;
    }
    size_t readBytes(uint8_t *buffer, size_t length) {
        return readBytes(reinterpret_cast<char *>(buffer), length);
    }
    size_t write(uint8_t) override {
        return 0;
    }
    size_t write(const uint8_t * /*buffer*/, size_t /*size*/) override {
        return 0;
    }
    operator bool() const {
        return false;
    }
};
