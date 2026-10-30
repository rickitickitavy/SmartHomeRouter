#pragma once

#include "GlobalSettings.h"
#include "DeviceTopicMap.h"
#include "SpiProtocol.h"
#include <Arduino.h>

class FoundDeviceList {
public:
    static constexpr int kMaxFound = 16;

    struct FoundDevice {
        uint8_t ieee[8];
        uint16_t shortAddr;
        uint8_t endpoint;
        char *manufacturer;
        char *model;
        uint8_t zigbeeType;
        bool used;
        FoundDevice *next;
    };

    FoundDeviceList();
    ~FoundDeviceList();

    void clear();
    // Found list stays empty until Search starts; only then may joins appear.
    void setAcceptingJoins(bool accepting);
    bool acceptingJoins() const { return joinsAccepted; }
    bool noteJoin(const SpiFrame &frame, DeviceTopicMap *registered);
    void noteIdentity(
        const uint8_t ieee[8],
        uint16_t shortAddr,
        uint8_t endpoint,
        const char *manufacturer,
        const char *model,
        DeviceTopicMap *registered,
        uint8_t zigbeeType = 0
    );
    uint8_t zigbeeTypeForIeee(const uint8_t ieee[8]) const;
    const char *manufacturerForIeee(const uint8_t ieee[8]) const;
    const char *modelForIeee(const uint8_t ieee[8]) const;
    uint16_t shortAddrForIeee(const uint8_t ieee[8]) const;
    void removeIeee(const uint8_t ieee[8]);
    String listJson(DeviceTopicMap *formatter);

private:
    FoundDevice *foundHead = nullptr;
    int foundCount = 0;
    bool joinsAccepted = false;

    FoundDevice *findByIeee(const uint8_t ieee[8]);
    const FoundDevice *findByIeee(const uint8_t ieee[8]) const;
    void freeFoundDevice(FoundDevice *device);
    FoundDevice *allocateFoundDevice();
};
