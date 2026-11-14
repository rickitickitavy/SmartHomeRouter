# Proposal

## Why

When host internal heap is nearly exhausted, AsyncWebServer/AsyncTCP and other services can wedge so the Mini-HA router becomes unreachable over HTTP even though the chip is still running. A hard fail-safe that frees the web stack, flushes history, and reboots recovers accessibility instead of leaving the device wedged. Operators need a Hardware-tunable trip threshold so boards with different free-heap floors can trip earlier or later without a firmware rebuild.

## What Changes

- On **P4 and S3 host** images: continuously (or frequently) watch **free internal heap**.
- System → Hardware gains **Low memory threshold** (integer **kB**): minimum **10**, maximum **32**, factory default **16**. Persist with hardware settings; GET/POST `/api/hardware`; include in global settings export/restore with the other hardware fields.
- When free heap is **less than or equal to** (`thresholdKb` × 1024) bytes:
  1. Stop application work that would keep allocating or serving (fail-safe latch; do not re-enter).
  2. **Stop, destroy, and free** the AsyncWebServer (and associated listen/connection state) to reclaim heap.
  3. **Save historical data** using the **current** history snapshot algorithm (same path as today’s pre-restart persist — LittleFS or SD backend as already configured).
  4. **Reboot** the host.
- Log the trip when logging is still possible.
- Does **not** apply to the slave image.

## Capabilities

### New Capabilities
- `low-memory-failsafe`: host low-heap detection (threshold from Hardware), ordered teardown → history flush → reboot recovery, plus Hardware UI/API for the threshold.

### Modified Capabilities
- *(Hardware UI / `/api/hardware` behavior is specified under `low-memory-failsafe` for this change; no separate web-console delta file in the change folder yet.)*

## Impact

- Host `main` loop / watchdog-style check; `WebConsole` teardown API (`server.end()` / destroy); invoke existing history `persistSnapshot` / pre-restart handler; `ESP.restart()`.
- `SettingsManager` + System → Hardware on the operator console; settings export/restore hardware slice.
- P4 and S3 host builds; slave unchanged.
- Touches AsyncWebServer lifecycle (intentional); avoid SPI OTA begin/chunk/end changes.
- Host version: bump **build** on apply from whatever is current; slave unchanged.
