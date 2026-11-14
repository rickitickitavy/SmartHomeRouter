# Proposal

## Why

Operators can see free RAM/PSRAM on Status but not how busy the host CPU is, so overload (SPI, MQTT, web) is hard to spot without a debugger. FreeRTOS run-time stats are already enabled; Status should expose a simple idle-based CPU % for the **master (host)** only.

## What Changes

- Sample CPU usage from FreeRTOS run-time / idle task counters (no per-feature timers across the app).
- **Host**: expose per-core busy percent (CPU0 and CPU1 on dual-core S3/P4) on the gateway status API.
- Status UI: show host CPU usage in a Status **CPU** group on **both** operator and simple-user Status views.
- Values are estimated over a short sampling window; show em dash / unavailable when a sample is not ready yet.
- **Out of scope:** slave CPU % and SPI status payload changes.

## Capabilities

### New Capabilities
- `controller-cpu-usage`: how the host computes and refreshes idle-based CPU busy percent for telemetry.

### Modified Capabilities
- `web-console`: Status shows host CPU usage from the gateway status API.

## Impact

- Host: small sampler + gateway status JSON fields; `data/index.html` and `data/user.html` Status widgets.
- Slave firmware unchanged; SPI OTA / InterChip status framing unchanged.
- Host build **4.2.79** (apply bump); slave unchanged.
