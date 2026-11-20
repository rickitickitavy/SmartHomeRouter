## Context

Slave (`esp32-c6-slave`) runs Zigbee ZCZR in a ZBOSS/Arduino Zigbee task and application logic on Arduino `loop` (`ZigbeeCoordinator::dispatch`, deferred SPI→radio commands). SPI answers on a separate high-priority task (`InterChipSlave`). Espressif requires `esp_zb_lock_acquire` before Zigbee SDK APIs except when already inside Zigbee callbacks.

Today:

- `sendZclPacket` uses a timed lock; ZDO `simple_desc` / `active_ep` acquire with 20 ms but **still call the API if acquire fails**.
- Neighbor walks (`esp_zb_nwk_get_next_neighbor`), address lookups (`esp_zb_address_short_by_ieee`, `esp_zb_ieee_address_by_short`), rebind bind requests, and other stack reads run from `loop` **without** the lock.
- Ping health = `checkResponsive()` = lock acquire/release only. Soft radio death keeps SPI and Ping green until a human resets the slave.

Order of work in this change: **A then B** in one apply.

Propose bumps: host **4.5.11**, slave **4.2.15**.

## Goals / Non-Goals

**Goals:**

- Make all non-callback Zigbee API use on the slave lock-correct; never call the stack after a failed acquire.
- Detect soft Zigbee death that leaves the mutex free and recover via the existing Ping → clear Zigbee flag → slave reboot / host EN reset path.
- Keep SPI responsive (no `portMAX_DELAY` Zigbee locks on the SPI task; short timed locks on `loop`/deferred radio paths).

**Non-Goals:**

- Changing SPI OTA fragile paths (`slave-spi-ota` / `firmware-update-process`).
- Using “no inbound reports for N minutes” alone as death (sleepy devices).
- Changing operator LED meanings or host Status UI.
- Host Zigbee radio (host has none).
- Fixing `CONFIG_REQUEST` always-after-10s (separate open change).

## Decisions

1. **A — Audit and wrap**  
   Inventory every `esp_zb_*` / ZDO / `zb_buf_*` / neighbor / address API used from slave application contexts that are not Zigbee callbacks. Hold the lock for the shortest critical section. Prefer one helper (e.g. try-lock + lambda, or explicit acquire/release) with a finite timeout (order of 20–100 ms, matching existing send path).  
   *Why:* Matches SDK contract; removes the race that can wedge the stack.

2. **A — Fail closed on lock miss**  
   If `esp_zb_lock_acquire` returns false, skip the API call and fail the operation soft (log at warning where useful). Especially fix `requestSimpleDescriptor` / `requestActiveEndpoints` that today call ZDO unlocked after a failed acquire.  
   *Why:* Unlocked call after failed acquire is worse than skipping one probe/send.

3. **A — Callbacks stay unlocked**  
   Paths already running on the Zigbee task (attribute report handlers, IAS callbacks, send-status callbacks invoked by the stack) MUST NOT take the lock again (deadlock risk). Document which entry points are callback vs `loop`.  
   *Why:* Espressif exception for Zigbee-task call sites; mirrors known freeze if lock taken inside callbacks.

4. **B — Buffer-pool probe in health**  
   `checkResponsive()` (or successor) MUST: acquire lock → `zb_buf_get_out` → if buffer id is 0, fail → else free buffer → release lock. Timeout on acquire remains finite (keep ~50 ms class).  
   *Why:* Soft hangs often leave the mutex free but exhaust or break the buffer pool; this catches more than lock-only without requiring inbound traffic.

5. **B — Consecutive send-failure streak**  
   Count consecutive `sendZclPacket` (and equivalent radio send) failures due to lock miss or no buffer. After **N** consecutive failures (propose **N = 5**) once Zigbee has been started longer than a grace (**60 s**), set the same unhealthy / `zigbeeLostRebootPending` path used by Ping. Success resets the streak.  
   *Why:* Catches TX-path death under command load without false resets on a quiet mesh.

6. **B — Keep host reset behavior**  
   When Ping reports Zigbee not started after it was started, host already logs and `enterReset()`. Slave already reboots on `consumeZigbeeLostReboot()`. Reuse those; do not invent a second reset channel.  
   *Why:* Minimal host change; proven recovery path.

7. **Timeouts**  
   Do not introduce `portMAX_DELAY` Zigbee locks on application/`loop` paths (historical hygiene change already removed that from ZCL send). SPI task must never wait on the Zigbee lock.

## Risks / Trade-offs

- **[Risk] Health false positives → slave reset loops** → Mitigation: grace after start; no quiet-mesh-only rule; buffer probe + send streak only; log clearly (`Zigbee health check failed on ping`, buffer/lock reasons).
- **[Risk] Lock contention slows `dispatch`** → Mitigation: short timeouts; skip work on miss; neighbor walk already periodic (~10 s).
- **[Risk] Missed API call site** → Mitigation: task checklist + grep audit of `esp_zb_`, `zb_buf_`, ZDO in slave coordinator; B as backstop.
- **[Risk] Fragile OTA interaction** → Mitigation: do not edit OTA SPI framing; if Zigbee health trips during OTA quiet, prefer existing OTA ignore rules for CONFIG_REQUEST-like paths—health reboot during active slave OTA should remain rare (OTA already clears deferred radio); do not widen OTA code.

## Migration Plan

- Ship in joined package with bumped slave (and host propose bump).
- No settings/schema migration.
- After flash, monitor for reduced soft hangs and for unexpected auto-resets; tune N/grace only if false positives appear (follow-up change).

## Open Questions

None blocking. Thresholds N=5 and grace=60 s are assumptions recorded here for apply; adjust only if field evidence shows false resets.
