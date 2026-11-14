# Design

## Context

See proposal.md. Host already reports `ESP.getFreeHeap()` on Status and flushes history via `SettingsManager::setPreRestartHandler` → `historyStore.persistSnapshot()`. `WebConsole` owns an `AsyncWebServer` with no dedicated public teardown today; STA rebind already uses `server.end()` / `server.begin()` patterns elsewhere in the async-web-server skill. Hardware settings already round-trip via GET/POST `/api/hardware` and the global settings export hardware slice.

## Goals / Non-Goals

**Goals:**
- Detect internal heap ≤ configured threshold (kB × 1024), with Hardware field **Low memory threshold**: **10–32** kB, default **16**.
- Ordered recovery: stop work → destroy/free AsyncWebServer → history snapshot (current algorithm) → reboot.
- P4 and S3 host only; one-shot latch per boot.
- Persist threshold with hardware settings; export/restore with peers.

**Non-Goals:**
- Using PSRAM free as the trip metric.
- Keeping HTTP reachable after teardown (recovery is via reboot).
- Slave-side fail-safe.
- Changing history snapshot binary format or SPI OTA framing.

## Decisions

1. **Metric**  
   `ESP.getFreeHeap()` (internal/default heap). Trip when `freeHeap <= lowMemoryThresholdKb * 1024`. Not PSRAM free — web/TCP pressure is internal DRAM.

2. **Threshold setting**  
   Integer field **Low memory threshold** in System → Hardware (kB). Range **10..32**, factory default **16**. API key e.g. `lowMemoryThresholdKb`. Reject out-of-range on POST; omitted on older clients keeps prior value or factory default. Same `isAdmin` Hardware gate as today. Include in settings export/restore with existing hardware fields.

3. **Where to poll**  
   Host main `loop` (and/or a low-frequency check already on the service path). Read the persisted threshold each check (or cached copy updated on Hardware save). Avoid allocating while checking. If already latched, return immediately.

4. **“Stop all process”**  
   Practical ordered stop before web destroy: stop accepting new work where cheap (MQTT loop idle, skip nonessential tasks, do not start new HTTP). Prefer calling existing stop/disconnect helpers over inventing a full RTOS task kill. Document concrete stop list in apply (WebConsole, MQTT client pump, deferred noncritical jobs). Do not block forever.

5. **AsyncWebServer free**  
   Add an explicit `WebConsole` teardown: `server.end()`, drop WebSocket clients if any, destroy/delete the `WebConsole` / server object so heap returns. Follow ESP32Async end semantics; do not `begin()` again before reboot.

6. **History**  
   Call the same persist used by pre-restart (`persistSnapshot` / pre-restart handler) so LittleFS vs SD backend stays whatever product already selected. Best-effort; then reboot.

7. **Reboot**  
   `ESP.restart()` after persist attempt. Optionally set a one-line reason log before teardown if LOGGER still works.

8. **Hysteresis**  
   None required: latch + reboot. No oscillation.

## Risks / Trade-offs

- **[Risk] Teardown itself needs heap** → Mitigation: tear down largest consumers first (web/TCP); avoid String growth; keep path allocation-free where possible.
- **[Risk] False trip under brief heap dips** → Mitigation: threshold floor is 10 kB (still extreme); optional double-check read before latch (same threshold twice) without delaying recovery much.
- **[Risk] History snapshot needs heap** → Mitigation: free web first; if persist fails, still reboot (spec).
- **[Risk] smtp-send / AsyncTCP interaction** → Mitigation: fail-safe is host recovery only; tear down web before restart; do not redesign SMTP worker.
- **[Risk] STA rebind vs destroy** → Mitigation: fail-safe path must not race a Wi-Fi rebind `begin()`; latch blocks rebind after trip.

## Migration Plan

- First boot / missing key: factory default **16** kB.
- Older clients that omit the field on POST: keep stored value or default.
- Rollback: older firmware ignores the latch logic and Hardware field.

## Open Questions

None blocking. Assumption: threshold values are whole **kilobytes** (× 1024) compared to free **internal** heap.
