# Tasks

## 1. Firmware version

- [x] 1.1 Bump `FIRMWARE_VERSION` build by 1 on apply (propose already set `3.6.02`) and verify `include/Defines.h` shows the apply build

## 2. Explicit TX power after coordinator start

- [x] 2.1 In `ZigbeeCoordinator::begin`, after successful `Zigbee.begin`, call `esp_zb_set_tx_power(20)` (or `static constexpr int8_t` equal to 20) — verify the call is only on the success path
- [x] 2.2 Read back with `esp_zb_get_tx_power` and log info including dBm; warn if read-back is not 20 — verify a successful slave start log shows the TX power line
- [x] 2.3 Include `esp_zigbee_core.h` (or the existing Zigbee include that exposes these APIs) if not already visible in `ZigbeeCoordinator.cpp` — verify the slave `esp32-c6-slave` env compiles

## Workflow follow-up

- Archive the change after review and apply are complete.
