# Design

## Context

See proposal.md. Scope narrowed to **host (master) only** — no slave CPU and no SPI status field. `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS` is already on. Status JSON is built in `main.cpp` (`hostGatewayStatusJson`); UI in `data/index.html` / `data/user.html`.

## Goals / Non-Goals

**Goals:**
- Idle-based busy % for host CPU0/CPU1.
- Status **CPU** group on operator + simple-user pages after Free memory.
- Reuse FreeRTOS counters only (no feature-level timers).

**Non-Goals:**
- Slave CPU telemetry or SPI status payload changes.
- Per-task breakdown UI or continuous high-rate tracing.
- Changing SPI OTA framing.

## Decisions

1. **Metric** — Over window Δt: `busy% ≈ 100 - round(100 * idle_delta / (total_delta / coreCount))` using IDLE0/IDLE1.
2. **Sampler** — `CpuUsageSampler` on host; `uxTaskGetSystemState`; refresh ~1 s from host loop; first window → unavailable.
3. **API** — `hostCpu0Percent`, `hostCpu1Percent` (integer or null). No `slaveCpuPercent`.
4. **UI** — Status group **CPU** (Host CPU0/CPU1) immediately after **Free memory**. **Devices** at the top of column 2. Sync both pages.
5. **Versions** — Host bump only.

## Risks / Trade-offs

- **[Risk] uxTaskGetSystemState allocates briefly** → Mitigation: call infrequently; internal heap; skip under pressure.
- **[Risk] Stats disabled** → Mitigation: gate on `configGENERATE_RUN_TIME_STATS`; show unavailable.

## Migration Plan

- Host-only flash; UI ignores missing keys on older firmware.
