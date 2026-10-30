#include "ActionCommandQueue.h"
#include "Defines.h"
#include "Logger.h"

#include <esp_heap_caps.h>
#include <string.h>

namespace {
constexpr UBaseType_t kActionCommandQueueLength = ACTION_COMMAND_QUEUE_MAX;
constexpr unsigned long kActionCommandDrainIntervalMs = 100UL;
constexpr size_t kDeviceCommandIeeeMax = 24;
constexpr size_t kDeviceCommandPayloadMax = 512;
}

ActionCommandQueue::ActionCommandQueue() = default;

bool ActionCommandQueue::begin() {
#if !BOARD_ROLE_HOST
    return false;
#else
    if (queueHandle != nullptr) {
        return true;
    }
    queueStorage = static_cast<uint8_t *>(
        heap_caps_malloc(kActionCommandQueueLength * sizeof(Job *), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );
    if (queueStorage == nullptr) {
        queueStorage = static_cast<uint8_t *>(ps_malloc(kActionCommandQueueLength * sizeof(Job *)));
    }
    if (queueStorage == nullptr) {
        LOGGER.warning("Action command queue storage alloc failed");
        return false;
    }
    queueHandle = xQueueCreateStatic(
        kActionCommandQueueLength,
        sizeof(Job *),
        queueStorage,
        &queueControl
    );
    if (queueHandle == nullptr) {
        LOGGER.warning("Action command queue create failed");
        return false;
    }
    return true;
#endif
}

void ActionCommandQueue::setHandlers(DeviceCommandFn deviceCommand, ApplyActionFn applyAction) {
    deviceCommandFn = deviceCommand;
    applyActionFn = applyAction;
}

char *ActionCommandQueue::duplicatePsramString(const char *source, size_t maxLength) {
    if (maxLength == 0) {
        return nullptr;
    }
    if (source == nullptr) {
        source = "";
    }
    size_t copyLen = strlen(source);
    if (copyLen >= maxLength) {
        copyLen = maxLength - 1;
    }
    char *copy = static_cast<char *>(
        heap_caps_malloc(copyLen + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );
    if (copy == nullptr) {
        copy = static_cast<char *>(ps_malloc(copyLen + 1));
    }
    if (copy == nullptr) {
        copy = static_cast<char *>(malloc(copyLen + 1));
    }
    if (copy == nullptr) {
        return nullptr;
    }
    memcpy(copy, source, copyLen);
    copy[copyLen] = '\0';
    return copy;
}

void ActionCommandQueue::freeJob(Job *job) {
    if (job == nullptr) {
        return;
    }
    if (job->kind == JobKind::DeviceCommand) {
        free(job->deviceCommand.ieeeText);
        free(job->deviceCommand.payload);
        job->deviceCommand.ieeeText = nullptr;
        job->deviceCommand.payload = nullptr;
    } else if (job->kind == JobKind::Action) {
        AutomationAction *action = job->action.action;
        job->action.action = nullptr;
        AutomationStore::freeActionList(action);
    }
    free(job);
}

bool ActionCommandQueue::enqueueJob(Job *job) {
#if !BOARD_ROLE_HOST
    freeJob(job);
    return false;
#else
    if (queueHandle == nullptr || job == nullptr) {
        freeJob(job);
        return false;
    }
    if (xQueueSend(queueHandle, &job, 0) != pdTRUE) {
        freeJob(job);
        return false;
    }
    return true;
#endif
}

bool ActionCommandQueue::enqueueDeviceCommand(const char *ieeeText, const char *payload, int channel) {
#if !BOARD_ROLE_HOST
    (void)ieeeText;
    (void)payload;
    (void)channel;
    return false;
#else
    if (queueHandle == nullptr) {
        return false;
    }
    Job *job = static_cast<Job *>(
        heap_caps_malloc(sizeof(Job), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );
    if (job == nullptr) {
        job = static_cast<Job *>(ps_malloc(sizeof(Job)));
    }
    if (job == nullptr) {
        job = static_cast<Job *>(malloc(sizeof(Job)));
    }
    if (job == nullptr) {
        return false;
    }
    memset(job, 0, sizeof(Job));
    job->kind = JobKind::DeviceCommand;
    job->deviceCommand.ieeeText = duplicatePsramString(ieeeText, kDeviceCommandIeeeMax);
    job->deviceCommand.payload = duplicatePsramString(payload, kDeviceCommandPayloadMax);
    job->deviceCommand.channel = channel;
    if (job->deviceCommand.ieeeText == nullptr || job->deviceCommand.payload == nullptr) {
        freeJob(job);
        return false;
    }
    return enqueueJob(job);
#endif
}

bool ActionCommandQueue::enqueueAction(
    AutomationStore *store,
    const AutomationAction *action,
    uint8_t depth
) {
#if !BOARD_ROLE_HOST
    (void)store;
    (void)action;
    (void)depth;
    return false;
#else
    if (queueHandle == nullptr || store == nullptr || action == nullptr) {
        return false;
    }
    AutomationAction *copy = nullptr;
    if (!store->cloneSingleAction(action, &copy) || copy == nullptr) {
        AutomationStore::freeActionList(copy);
        return false;
    }
    Job *job = static_cast<Job *>(
        heap_caps_malloc(sizeof(Job), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );
    if (job == nullptr) {
        job = static_cast<Job *>(ps_malloc(sizeof(Job)));
    }
    if (job == nullptr) {
        job = static_cast<Job *>(malloc(sizeof(Job)));
    }
    if (job == nullptr) {
        AutomationStore::freeActionList(copy);
        return false;
    }
    memset(job, 0, sizeof(Job));
    job->kind = JobKind::Action;
    job->action.action = copy;
    job->action.depth = depth;
    return enqueueJob(job);
#endif
}

uint32_t ActionCommandQueue::busyCount() const {
#if !BOARD_ROLE_HOST
    return 0;
#else
    if (queueHandle == nullptr) {
        return 0;
    }
    return (uint32_t)uxQueueMessagesWaiting(queueHandle);
#endif
}

void ActionCommandQueue::tick() {
#if !BOARD_ROLE_HOST
    return;
#else
    if (queueHandle == nullptr) {
        return;
    }
    const unsigned long nowMs = millis();
    if (nextDrainMs != 0 && (long)(nowMs - nextDrainMs) < 0) {
        return;
    }
    Job *job = nullptr;
    if (xQueueReceive(queueHandle, &job, 0) != pdTRUE || job == nullptr) {
        return;
    }
    if (job->kind == JobKind::DeviceCommand) {
        if (deviceCommandFn != nullptr) {
            deviceCommandFn(
                job->deviceCommand.ieeeText,
                job->deviceCommand.payload,
                job->deviceCommand.channel
            );
        }
    } else if (job->kind == JobKind::Action) {
        if (applyActionFn != nullptr && job->action.action != nullptr) {
            applyActionFn(job->action.action, job->action.depth);
        }
    }
    freeJob(job);
    nextDrainMs = nowMs + kActionCommandDrainIntervalMs;
#endif
}
