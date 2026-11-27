#pragma once

#include "WiFi.h"

class WiFiUDP {
public:
    bool begin(uint16_t /*port*/) {
        return false;
    }
    bool beginMulticast(IPAddress /*address*/, uint16_t /*port*/) {
        return false;
    }
    bool beginMulticast(IPAddress /*local*/, IPAddress /*multicast*/, uint16_t /*port*/) {
        return false;
    }
    bool beginMulticast(IPAddress /*local*/, IPAddress /*multicast*/, int /*port*/) {
        return false;
    }
    void stop() {}
    int beginPacket(const char * /*host*/, uint16_t /*port*/) {
        return 0;
    }
    int beginPacket(IPAddress /*ip*/, uint16_t /*port*/) {
        return 0;
    }
    int endPacket() {
        return 0;
    }
    size_t write(const uint8_t * /*buffer*/, size_t /*size*/) {
        return 0;
    }
    size_t write(uint8_t) {
        return 0;
    }
    int parsePacket() {
        return 0;
    }
    int available() {
        return 0;
    }
    int read(char * /*buffer*/, size_t /*len*/) {
        return 0;
    }
    int read(uint8_t *buffer, size_t len) {
        return read(reinterpret_cast<char *>(buffer), len);
    }
    IPAddress remoteIP() {
        return IPAddress();
    }
    uint16_t remotePort() {
        return 0;
    }
};
