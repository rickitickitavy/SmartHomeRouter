# Proposal

## Why

When host internal heap is nearly exhausted, AsyncWebServer/AsyncTCP and other services can wedge so the Mini-HA router becomes unreachable over HTTP even though the chip is still running. A hard fail-safe that frees the web stack, flushes history, and reboots recovers accessibility instead of leaving the device wedged.

## What Changes

- On **P4 and S3 host** images: continuously (or frequently) watch **free internal heap**.
- When free heap is **less than or equal to 15 kB** (15 × 1024 bytes):
  1. Stop application work that would keep allocating or serving (fail-safe latch; do not re-enter).
  2. **Stop, destroy, and free** the AsyncWebServer (and associated listen/connection state) to reclaim heap.
  3. **Save historical data** using the **current** history snapshot algorithm (same path as today’s pre-restart persist — LittleFS or SD backend as already configured).
  4. **Reboot** the host.
- Log the trip when logging is still possible.
- Does **not** apply to the slave image.
- Threshold is fixed at 15 kB for this change (not a Hardware setting unless added later).

## Capabilities

### New Capabilities
- `low-memory-failsafe`: host low-heap detection and ordered teardown → history flush → reboot recovery.

### Modified Capabilities
- *(none required at requirement level beyond the new capability; history flush reuses existing historical-data persist behavior)*

## Impact

- Host `main` loop / watchdog-style check; `WebConsole` teardown API (`server.end()` / destroy); invoke existing history `persistSnapshot` / pre-restart handler; `ESP.restart()`.
- P4 and S3 host builds; slave unchanged.
- Touches AsyncWebServer lifecycle (intentional); avoid SPI OTA begin/chunk/end changes.
- Host version **4.2.64** (propose); slave unchanged.
