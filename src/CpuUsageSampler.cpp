#include "CpuUsageSampler.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>
#include <string.h>

namespace {

constexpr unsigned long kMinSampleIntervalMs = 1000;

bool taskNameStartsWithIdle(const char *name) {
    return name != nullptr
        && (name[0] == 'I' || name[0] == 'i')
        && (name[1] == 'D' || name[1] == 'd')
        && (name[2] == 'L' || name[2] == 'l')
        && (name[3] == 'E' || name[3] == 'e');
}

// ESP_TIMER run-time clock is wall time: each core's idle counter advances at the
// same rate as ulTotalRunTime while that core is idle. Do NOT divide total by
// core count (that made idleDelta >= share and forced 0% busy).
uint8_t busyFromIdleShare(uint32_t idleDelta, uint32_t totalDelta) {
    if (totalDelta == 0) {
        return 0;
    }
    if (idleDelta >= totalDelta) {
        return 0;
    }
    const uint32_t busy =
        (100UL * (totalDelta - idleDelta) + (totalDelta / 2UL)) / totalDelta;
    if (busy > 100UL) {
        return 100;
    }
    return (uint8_t)busy;
}

}  // namespace

void CpuUsageSampler::begin() {
    haveBaseline = false;
    haveCpu0 = false;
    haveCpu1 = false;
    percentCpu0 = 0;
    percentCpu1 = 0;
    lastTotalRuntime = 0;
    lastIdle0Runtime = 0;
    lastIdle1Runtime = 0;
    lastSampleMs = 0;
}

void CpuUsageSampler::sample() {
#if (configGENERATE_RUN_TIME_STATS != 1)
    haveCpu0 = false;
    haveCpu1 = false;
    return;
#else
    const unsigned long nowMs = millis();
    if (lastSampleMs != 0 && (nowMs - lastSampleMs) < kMinSampleIntervalMs) {
        return;
    }
    lastSampleMs = nowMs;

    UBaseType_t taskCount = uxTaskGetNumberOfTasks();
    if (taskCount == 0 || taskCount > 64) {
        return;
    }
    TaskStatus_t *tasks = (TaskStatus_t *)heap_caps_malloc(
        sizeof(TaskStatus_t) * taskCount,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT
    );
    if (tasks == nullptr) {
        return;
    }

    uint32_t totalRuntime = 0;
    const UBaseType_t got = uxTaskGetSystemState(tasks, taskCount, &totalRuntime);
    uint32_t idle0Runtime = 0;
    uint32_t idle1Runtime = 0;
    bool foundIdle0 = false;
    bool foundIdle1 = false;
    for (UBaseType_t index = 0; index < got; index++) {
        const char *taskName = tasks[index].pcTaskName;
        if (!taskNameStartsWithIdle(taskName)) {
            continue;
        }
        int coreId = -1;
#if (configTASKLIST_INCLUDE_COREID == 1)
        coreId = (int)tasks[index].xCoreID;
#endif
        // Prefer explicit IDLE0/IDLE1 suffix; else use core id; else first→0 second→1.
        const char suffix = taskName[4];
        if (suffix == '0' || coreId == 0) {
            idle0Runtime = tasks[index].ulRunTimeCounter;
            foundIdle0 = true;
        } else if (suffix == '1' || coreId == 1) {
            idle1Runtime = tasks[index].ulRunTimeCounter;
            foundIdle1 = true;
        } else if (!foundIdle0) {
            idle0Runtime = tasks[index].ulRunTimeCounter;
            foundIdle0 = true;
        } else if (!foundIdle1) {
            idle1Runtime = tasks[index].ulRunTimeCounter;
            foundIdle1 = true;
        }
    }
    heap_caps_free(tasks);

    if (!foundIdle0 || totalRuntime == 0) {
        return;
    }

    if (!haveBaseline) {
        lastTotalRuntime = totalRuntime;
        lastIdle0Runtime = idle0Runtime;
        lastIdle1Runtime = idle1Runtime;
        haveBaseline = true;
        haveCpu0 = false;
        haveCpu1 = false;
        return;
    }

    if (totalRuntime < lastTotalRuntime
        || idle0Runtime < lastIdle0Runtime
        || (foundIdle1 && idle1Runtime < lastIdle1Runtime)) {
        lastTotalRuntime = totalRuntime;
        lastIdle0Runtime = idle0Runtime;
        lastIdle1Runtime = idle1Runtime;
        haveCpu0 = false;
        haveCpu1 = false;
        return;
    }

    const uint32_t totalDelta = totalRuntime - lastTotalRuntime;
    const uint32_t idle0Delta = idle0Runtime - lastIdle0Runtime;
    const uint32_t idle1Delta = foundIdle1 ? (idle1Runtime - lastIdle1Runtime) : 0;
    lastTotalRuntime = totalRuntime;
    lastIdle0Runtime = idle0Runtime;
    lastIdle1Runtime = idle1Runtime;

    if (totalDelta == 0) {
        return;
    }

    percentCpu0 = busyFromIdleShare(idle0Delta, totalDelta);
    haveCpu0 = true;
    if (foundIdle1 && portNUM_PROCESSORS > 1) {
        percentCpu1 = busyFromIdleShare(idle1Delta, totalDelta);
        haveCpu1 = true;
    } else {
        haveCpu1 = false;
    }
#endif
}
