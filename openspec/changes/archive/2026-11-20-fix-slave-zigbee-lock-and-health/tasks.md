## 1. A — Lock audit and wrappers

- [x] 1.1 Grep slave Zigbee paths (`ZigbeeCoordinator`, related slave helpers) for `esp_zb_`, `zb_buf_`, ZDO, neighbor, and address APIs; classify each as Zigbee-callback vs `loop`/deferred/Ping
- [x] 1.2 Add a small timed try-lock helper (or consistent acquire/release pattern) with finite timeout; never use `portMAX_DELAY` on `loop` or SPI task
- [x] 1.3 Wrap all non-callback neighbor walks, IEEE/short lookups, rebind bind requests, and other unlocked stack reads/writes under the lock
- [x] 1.4 Fix ZDO Simple Descriptor / Active Endpoints (and any similar) so a failed acquire **skips** the API call (no unlocked call)
- [x] 1.5 Confirm Zigbee callbacks do not re-acquire the lock; leave callback-context calls unlocked
- [x] 1.6 Keep ZCL send on a finite lock timeout; soft-fail on miss

## 2. B — Stronger health and recovery

- [x] 2.1 Extend Ping health (`checkResponsive` or successor): under one lock hold, require successful `zb_buf_get_out` then free; fail health on lock miss or buffer 0
- [x] 2.2 On health failure: clear Zigbee-started for Pong and set existing `zigbeeLostRebootPending` / reboot path; keep clear logs
- [x] 2.3 Add consecutive send-failure streak (lock miss or no buffer) with post-start grace 60 s and threshold 5; success resets streak; trip same lost path; do not use quiet-inbound-only death
- [x] 2.4 Verify host still resets slave when Ping clears Zigbee-started (no host redesign); touch host only if a log/comment is required

## 3. Versions and smoke

- [x] 3.1 Confirm propose bumps: host **4.5.11**, slave **4.2.15**; bump again on apply if the apply skill requires a further build tick for edited sources
- [x] 3.2 Build `esp32-c6-slave` successfully; do not flash unless the human approves upload
- [x] 3.3 Smoke: Ping health still green after normal start; forced buffer/lock failure path logs and clears Zigbee bit; unlocked-call sites gone from non-callback paths (grep review)
