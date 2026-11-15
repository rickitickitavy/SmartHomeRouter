# Tasks

## 1. Protocol

- [x] 1.1 Add `SpiEvtConfigRequest` (or equivalent name) to `include/SpiProtocol.h` with the next free slave→host event id; document it next to other `SpiEvt*` entries
- [x] 1.2 Confirm no conflict with OTA / status / device-map event ids

## 2. Slave (shared)

- [x] 2.1 On SPI-ready / first `SLAVE_READY` arming, start a **10s** one-shot deadline for full-config request (`InterChipSlave`)
- [x] 2.2 When the deadline fires, enqueue `SpiEvtConfigRequest` once per boot; log clearly (e.g. `CONFIG_REQUEST queued`)
- [x] 2.3 Ensure the request uses the same outbound SPI event queue / IRQ path as `SLAVE_READY`
- [x] 2.4 Bump `FIRMWARE_VERSION_SLAVE` build on apply (propose already set **4.2.10**)

## 3. Host (S3 + P4)

- [x] 3.1 Handle `SpiEvtConfigRequest` in `InterChipHost`: when not updating slave via OTA, clear settings-queued state as needed and call `maybePushSettings()` (same settings path as READY configure)
- [x] 3.2 Verify existing `SpiEvtSettingsOk` → `publishHostStores` in `main.cpp` still runs so devices/users are pushed after settings acceptance (no parallel registry channel)
- [x] 3.3 Accept the request while already in `HostBringupNormal` (and when WaitReady can push settings), on both S3 and P4 hosts
- [x] 3.4 Log the request and re-push clearly for field diagnosis
- [x] 3.5 Bump `FIRMWARE_VERSION_HOST` build on apply (propose already set **4.3.04**)

## 4. Fragile-zone gate

- [x] 4.1 Before editing InterChip / SpiProtocol sources, obtain explicit approval naming `slave-spi-ota` (and treat `firmware-update-process` as parent if required by project hooks)
- [x] 4.2 Do not change SPI OTA begin/chunk/end, ACK/retry, or apply-drain logic

## 5. Verification

- [x] 5.1 Joint boot (host + slave): after ~10s, config request may re-run configure; slave ends with settings + full device/user map; Zigbee starts as today
- [x] 5.2 Slave-only abnormal reboot while host stays up: within ~10s after slave SPI-ready, host re-pushes settings and stores; slave registry is populated again without requiring a host reboot
- [x] 5.3 Confirm S3 and P4 host builds both include the handler
- [x] 5.4 Smoke: active slave OTA does not break when a late config request arrives (ignored/deferred)
