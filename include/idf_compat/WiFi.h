#pragma once

#include "Arduino.h"

enum wifi_mode_t {
    WIFI_OFF = 0,
    WIFI_STA = 1,
    WIFI_AP = 2,
    WIFI_AP_STA = 3
};

enum wl_status_t {
    WL_IDLE_STATUS = 0,
    WL_NO_SSID_AVAIL = 1,
    WL_SCAN_COMPLETED = 2,
    WL_CONNECTED = 3,
    WL_CONNECT_FAILED = 4,
    WL_CONNECTION_LOST = 5,
    WL_DISCONNECTED = 6
};

class IPAddress {
public:
    IPAddress() {
        memset(octets, 0, sizeof(octets));
    }

    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
        octets[0] = a;
        octets[1] = b;
        octets[2] = c;
        octets[3] = d;
    }

    explicit IPAddress(uint32_t address) {
        octets[0] = (uint8_t)(address & 0xFF);
        octets[1] = (uint8_t)((address >> 8) & 0xFF);
        octets[2] = (uint8_t)((address >> 16) & 0xFF);
        octets[3] = (uint8_t)((address >> 24) & 0xFF);
    }

    uint8_t operator[](int index) const {
        if (index < 0 || index > 3) {
            return 0;
        }
        return octets[index];
    }

    uint8_t &operator[](int index) {
        static uint8_t sink = 0;
        if (index < 0 || index > 3) {
            return sink;
        }
        return octets[index];
    }

    bool fromString(const char *text) {
        if (text == nullptr) {
            return false;
        }
        unsigned a = 0;
        unsigned b = 0;
        unsigned c = 0;
        unsigned d = 0;
        if (sscanf(text, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
            return false;
        }
        if (a > 255 || b > 255 || c > 255 || d > 255) {
            return false;
        }
        octets[0] = (uint8_t)a;
        octets[1] = (uint8_t)b;
        octets[2] = (uint8_t)c;
        octets[3] = (uint8_t)d;
        return true;
    }

    String toString() const {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "%u.%u.%u.%u", octets[0], octets[1], octets[2], octets[3]);
        return String(tmp);
    }

    operator uint32_t() const {
        return (uint32_t)octets[0]
            | ((uint32_t)octets[1] << 8)
            | ((uint32_t)octets[2] << 16)
            | ((uint32_t)octets[3] << 24);
    }

    uint8_t octets[4];
};

class WiFiClass {
public:
    wifi_mode_t getMode() {
        return WIFI_AP;
    }
    bool mode(wifi_mode_t) {
        return true;
    }
    void persistent(bool) {}
    wl_status_t status() {
        return WL_DISCONNECTED;
    }
    IPAddress localIP() {
        return IPAddress();
    }
    IPAddress softAPIP() {
        return IPAddress();
    }
    int RSSI() {
        return 0;
    }
    bool disconnect(bool = true) {
        return true;
    }
    bool disconnect(bool, bool, int) {
        return true;
    }
};

inline WiFiClass WiFi;
