# Proposal

## Why

Zigbee TX power on the ESP32-C6 slave is never set in application code; the stack default happens to be +20 dBm, but that is implicit. Operators want an **explicit** +20 dBm setting after the coordinator starts so power cannot silently drift if stack defaults change.

## What Changes

- After a successful slave Zigbee coordinator start, the firmware MUST call `esp_zb_set_tx_power(20)` (dBm).
- Log the configured (and, if available, read-back) TX power at info level so operators can confirm it in the slave log.
- No web UI, settings field, or runtime operator control in this change.
- RX sensitivity is out of scope (hardware limit; not software-configurable).

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `zigbee-slave-radio`: after coordinator start succeeds, IEEE 802.15.4 TX power MUST be set explicitly to **+20 dBm**.

## Impact

- Slave only: `src/ZigbeeCoordinator.cpp` (`ZigbeeCoordinator::begin` after `Zigbee.begin` succeeds).
- Host image unchanged for this behavior.
- `FIRMWARE_VERSION` propose bump `3.6.01` → `3.6.02` (apply bumps again when source lands).
