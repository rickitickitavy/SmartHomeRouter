#pragma once

#include "AutomationStore.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <stdint.h>

class ActionCommandQueue {
public:
    using DeviceCommandFn = int (*)(const char *ieeeText, const char *payload, int channel);
    using ApplyActionFn = void (*)(const AutomationAction *action, uint8_t depth);

    ActionCommandQueue();

    bool begin();
    void setHandlers(DeviceCommandFn deviceCommand, ApplyActionFn applyAction);
    bool enqueueDeviceCommand(const char *ieeeText, const char *payload, int channel);
    bool enqueueAction(AutomationStore *store, const AutomationAction *action, uint8_t depth);
    void tick();
    uint32_t busyCount() const;

private:
    enum class JobKind : uint8_t {
        DeviceCommand = 0,
        Action = 1
    };

    struct DeviceCommandJob {
        char *ieeeText;
        char *payload;
        int channel;
    };

    struct ActionJob {
        AutomationAction *action;
        uint8_t depth;
    };

    struct Job {
        JobKind kind;
        union {
            DeviceCommandJob deviceCommand;
            ActionJob action;
        };
    };

    QueueHandle_t queueHandle = nullptr;
    StaticQueue_t queueControl{};
    uint8_t *queueStorage = nullptr;
    DeviceCommandFn deviceCommandFn = nullptr;
    ApplyActionFn applyActionFn = nullptr;
    unsigned long nextDrainMs = 0;

    static char *duplicatePsramString(const char *source, size_t maxLength);
    static void freeJob(Job *job);
    bool enqueueJob(Job *job);
};
