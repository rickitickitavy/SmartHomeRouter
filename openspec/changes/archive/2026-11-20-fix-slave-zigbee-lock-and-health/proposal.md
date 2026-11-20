## Why

Periodically (about 1–4 times per day) the ESP32-C6 slave stops serving Zigbee (no inbound LED activity, devices dead) while SPI still answers the host. Manual slave reset recovers. Investigation shows the Ping “Zigbee health” check only tries `esp_zb_lock_acquire(50ms)`, so a soft radio/stack hang that leaves the mutex free is invisible. Application code also calls many Zigbee SDK APIs from the Arduino `loop` task without holding `esp_zb_lock` (and sometimes still calls ZDO APIs after a failed timed acquire). Espressif documents those APIs as not thread-safe outside Zigbee callbacks—matching this failure mode.

## What Changes

- **A (primary):** From non-callback contexts, every Zigbee SDK / ZBOSS call used by the slave coordinator MUST run under `esp_zb_lock_acquire` / `esp_zb_lock_release`. If acquire fails, the call MUST NOT proceed (fix ZDO paths that still invoke the stack after a failed 20 ms lock).
- **B (safety net):** Strengthen Ping health beyond “mutex free”: under the same lock hold, prove the buffer pool with `zb_buf_get_out` (then free). Mark Zigbee unhealthy and use the existing slave reboot + host “Zigbee lost on ping → reset slave” path when health fails. Also trip unhealthy after a short streak of consecutive radio send failures (lock or no buffer), with a post-start grace window. Do **not** treat “no inbound for N minutes” alone as death (sleepy end devices).
- Slave firmware build bump (propose **4.2.15**); host bump for propose rule (**4.5.11**) even if apply stays slave-heavy.

## Capabilities

### Modified Capabilities

- `zigbee-slave-radio`: thread-safe Zigbee API use from `loop`/deferred paths; stronger radio liveness for health.
- `host-slave-spi`: Ping health semantics when Zigbee is “started” must reflect the stronger check; existing host reset-on-lost-Zigbee-flag stays.

## Impact

- Slave: `ZigbeeCoordinator` (and any other non-callback `esp_zb_*` call sites on the slave), `InterChipSlave` Ping health / reboot path, possibly `main.cpp` health wiring.
- Host: no intentional product UX change; continues to reset slave when Ping clears `SPI_STATUS_FLAG_ZIGBEE_STARTED`.
- Fragile zones `slave-spi-ota` / `firmware-update-process`: do **not** change SPI OTA begin/chunk/end, ACK/retry, or apply-drain. Zigbee lock/health work must stay outside those paths.
- Risk: overly aggressive health → false slave resets; mitigated by buffer-pool probe + send-failure streak (not quiet-mesh timeout) and post-start grace.
