# Design

## Context

See proposal.md. `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS` and `CONFIG_FREERTOS_RUN_TIME_STATS_USING_ESP_TIMER` are already on in `sdkconfig.defaults`. Status JSON is built in `main.cpp` (`gatewayStatusJson`); UI in `data/index.html` / `data/user.html`. Host polls slave status about every `InterChipHost::kStatusPeriodMs` (10 s).

## Goals / Non-Goals

**Goals:**
- Idle-based busy % for host CPU0/CPU1 and slave CPU.
- Status **CPU** group on operator + simple-user pages.
- Reuse FreeRTOS counters only (no feature-level timers).

**Non-Goals:**
- Per-task breakdown UI, flame graphs, or continuous high-rate tracing.
- Changing SPI OTA framing or fragile OTA zones beyond adding a field on the existing status reply.
- Exact cycle-accurate hardware PMU metrics.

## Decisions

1. **Metric**  
   Over window Δt: `busy% = 100 - round(100 * idle_runtime_delta / total_runtime_delta)` (or sum of idle tasks vs sum of all tasks). Dual-core host: use IDLE0/IDLE1 (or core affinity) for CPU0/CPU1. Slave (single HP core on C6 app): one percent.

2. **Sampler**  
   Shared small helper (host + slave copies or thin shared header): snapshot via `uxTaskGetSystemState`, store previous counters, compute on next call. Refresh every ~1–2 s on host loop; slave refresh each status build or on a 1–2 s tick. First window → unavailable.

3. **API fields** (gateway status JSON)  
   - `hostCpu0Percent`, `hostCpu1Percent` (integers or null)  
   - `slaveCpuPercent` (integer or null)  
   Omit or null when unavailable.

4. **SPI**  
   Extend existing slave status text/binary fields with one CPU percent (prefer a compact tagged field consistent with current status parsing). Host caches last good value; on unlink show unavailable.

5. **UI**  
   New Status group **CPU** after **Free memory**: rows Host CPU0, Host CPU1, Slave CPU. Sync `index.html` and `user.html`.

6. **Versions**  
   Propose bumped host **4.2.59**, slave **4.2.07**.

## Risks / Trade-offs

- **[Risk] uxTaskGetSystemState allocates briefly** → Mitigation: call infrequently; use stack/PSRAM carefully; skip under heap pressure.
- **[Risk] Stats disabled on a board env** → Mitigation: verify sdkconfig for P4/S3/C6; show unavailable if macros off.
- **[Risk] SPI status parse churn** → Mitigation: additive field; ignore unknown on old peers during mixed versions.
- **[Risk] 10 s slave status cadence feels stale** → Mitigation: slave keeps its own 1–2 s sample so each status reply is fresh enough; optional faster later.

## Migration Plan

- Roll host+slave together for full Status; old slave without CPU field → host shows slave CPU as unavailable until both updated.
- Rollback: older UI ignores unknown JSON keys.

## Open Questions

None blocking. Assumption: both host and slave appear in Status (from “controllers” request); host dual-core shows two rows.
