# Design

## Context

See proposal.md — Why. Today `AutomationEngine::applyAction` / `runActions` run inline on trigger, timer, nested RUN, and `POST /api/consoles/run-action`. Base UI control shares `applyManualDeviceCommand` via `POST /api/devices/command` (all kinds: On/Off, covering, write-attr bodies, cluster/custom). Configure uses a separate `writeAttribute` path. Email already has its own PSRAM FreeRTOS queue (`EmailSender`); Wi‑Fi MQTT commands use an 8-slot internal pending list. Status already exposes `emailQueue`; Hardware persists SPI + LED brightness in `GlobalSettings.reserved[]` (offsets 0–3 SPI, 4 theme leftover, 5–6 LEDs). `FIRMWARE_VERSION` propose bump is `3.9.15`.

## Goals / Non-Goals

**Goals:**

- One application-level PSRAM FIFO (max 1024) for automation action units and optional UI base commands.
- Main-loop drain with ≥100 ms between started executions; overflow drops newest.
- Persist Hardware boolean; Status Free memory busy count on operator + simple-user.
- Keep configure and lower-layer SPI/email queues out of this module’s scope.

**Non-Goals:**

- Changing Zigbee retry/ACK policy or SPI frame pacing inside `ZigbeeSpiProxy`.
- Guaranteeing delivery (queue only spaces issuance).
- Logging every overflow (silent drop is enough unless debugging is added later).
- Simple-user Hardware UI (admin-only System → Hardware unchanged for who can edit).
- Replacing `EmailSender`’s SMTP queue (email actions still enqueue SMTP after FIFO applies the email action).

## Decisions

1. **Single FIFO for two producer kinds**  
   Elements are tagged: `ActionJob` (deep-copied or owned `AutomationAction` snapshot sufficient to call existing apply) vs `DeviceCommandJob` (ieee/topic id, channel, payload string). One drain loop, one busy count.  
   *Alternatives considered:* Separate queues (two Status counters, harder fairness); only wrap `applyManualDeviceCommand` (would miss automation-control and email apply ordering).

2. **Always queue automation actions; gate only UI base commands**  
   Matches the Hardware flag wording. `runActions` / `runAction` become enqueue-of-copies; nested RUN that schedules more actions enqueues onto the same FIFO (existing depth limit still applies when expanding).  
   *Alternatives considered:* Queue only device-target actions (rejected — user asked for action execution refactor).

3. **PSRAM + static FreeRTOS queue like EmailSender**  
   `xQueueCreateStatic` with storage from `heap_caps_malloc(MALLOC_CAP_SPIRAM)` or a static SPIRAM-capable buffer for 1024 pointers/slots; job payloads in PSRAM. Cap `ACTION_COMMAND_QUEUE_MAX 1024` in `Defines.h`.  
   *Alternatives considered:* Ring in internal RAM (too large for 1024 rich jobs); dynamic only (user allowed static).

4. **Drain in host `loop()` with `millis()` deadline**  
   After starting one job, set `nextDrainMs = now + 100`. Do not block the loop for 100 ms. Keep `delay(5)` as today.  
   *Alternatives considered:* FreeRTOS timer task (extra core/priority coupling; email already uses a worker — keep this queue on main for SPI/command affinity).

5. **Hardware flag in `reserved[7]`**  
   One byte boolean, default 0 (false). Expose as `useQueueForBaseDeviceCommand` on GET/POST `/api/hardware` and settings export slice if hardware is already exported. UI: `.field-check` with bare checkbox (square only).  
   *Alternatives considered:* New EEPROM group (overkill for one bit).

6. **Status field name**  
   Add numeric field alongside memory (e.g. `actionQueue` or `commandQueueBusy`) in `hostGatewayStatusJson`; Free memory row label e.g. **Action queue**. Mirror in `data/index.html` and `data/user.html`.  
   *Alternatives considered:* Put under E-mail group (wrong semantics).

7. **HTTP success when enqueueing**  
   Queued UI commands return success when accepted into the FIFO; full queue → ignore and return a failure or quiet drop with non-2xx/false — prefer explicit failure on UI enqueue so the operator knows (overflow ignore still means not stored). Automation trigger path silently drops on overflow.  
   *Assumption recorded:* UI POST may report failure when drop; automation path stays silent.

8. **Group On/Off with flag on**  
   Each per-device OFF/ON is a separate FIFO element so spacing applies across the group burst (all kinds if group expands later — today On/Off only for group control).

## Risks / Trade-offs

- **[Risk] Deep-copy of rich `AutomationAction` (email strings) under burst → Mitigation:** allocate job payloads in PSRAM; fail enqueue on OOM same as overflow.  
- **[Risk] 100 ms × N adds latency for large action lists → Mitigation:** accepted pacing trade-off; capacity 1024.  
- **[Risk] Nested RUN floods queue → Mitigation:** keep existing depth limit; overflow drops.  
- **[Risk] Dual path bugs (flag off still queues / configure accidentally queued) → Mitigation:** configure never calls the new enqueue API; flag checked only at UI command entry.  
- **[Trade-off] Silent automation overflow →** lost actions under extreme load; Status busy count helps operators see pressure before full.

## Migration Plan

- Default flag false → behavior for UI commands unchanged; only automation/console action timing changes (paced).  
- No store format version bump beyond reserved byte (0 = false).  
- Rollback: disable flag; remove drain (not needed if flag false still queues actions — rollback is firmware revert).

## Open Questions

None that block specs or tasks; UI enqueue failure code on overflow can follow existing `/api/devices/command` error style during apply.
