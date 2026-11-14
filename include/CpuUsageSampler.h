#pragma once

#include <stdint.h>

// Idle-based busy percent from FreeRTOS run-time stats (host Status telemetry).
class CpuUsageSampler {
public:
    void begin();
    // Refresh counters; call about every 1–2 s from the host loop.
    void sample();

    bool cpu0Ready() const { return haveCpu0; }
    bool cpu1Ready() const { return haveCpu1; }
    uint8_t cpu0Percent() const { return percentCpu0; }
    uint8_t cpu1Percent() const { return percentCpu1; }

private:
    bool haveBaseline = false;
    bool haveCpu0 = false;
    bool haveCpu1 = false;
    uint8_t percentCpu0 = 0;
    uint8_t percentCpu1 = 0;
    uint32_t lastTotalRuntime = 0;
    uint32_t lastIdle0Runtime = 0;
    uint32_t lastIdle1Runtime = 0;
    unsigned long lastSampleMs = 0;
};
