# Proposal

## Why

Operators can see free RAM/PSRAM on Status but not how busy the host and slave CPUs are, so overload (SPI, MQTT, web, Zigbee) is hard to spot without a debugger. FreeRTOS run-time stats are already enabled; Status should expose a simple idle-based CPU % in the console widget.

## What Changes

- Sample CPU usage from FreeRTOS run-time / idle task counters (no per-feature timers across the app).
- **Host**: expose per-core busy percent (CPU0 and CPU1 on dual-core S3/P4) on the gateway status API.
- **Slave**: sample one busy percent on the C6 and include it in the existing host←slave status path so the host can cache it.
- Status UI: show host (and slave when known) CPU usage in a Status group on **both** operator and simple-user Status views (same sync rule as other Status fields).
- Values are estimated over a short sampling window; show em dash / unavailable when a sample is not ready yet or the slave link is down.

## Capabilities

### New Capabilities
- `controller-cpu-usage`: how host and slave compute and refresh idle-based CPU busy percent for telemetry.

### Modified Capabilities
- `web-console`: Status shows host/slave CPU usage from the gateway status API.
- `host-slave-spi`: slave status replies carry slave CPU busy percent for the host cache.

## Impact

- Host: small sampler + `/api/status` (or existing gateway status JSON) fields; `data/index.html` and `data/user.html` Status widgets.
- Slave: same-style sampler; status payload field; SPI status parsing on host (`InterChipHost` / proxy). Does **not** change SPI OTA begin/chunk/end framing.
- sdkconfig already has `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS`; keep/confirm on both images.
- Host build **4.2.59**, slave **4.2.07** (propose bump).
