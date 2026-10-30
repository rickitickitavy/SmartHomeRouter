# Tasks

## 1. Host device record + JSON

- [x] 1.1 Add last-known NWK short on `DeviceTopicEntry`, default `0` on create/clone/clear, and verify load of existing `devices.json` without `nwk` leaves short `0`
- [x] 1.2 Serialize/parse `nwk` in device list/store JSON and verify round-trip of a non-zero short after save/reload
- [x] 1.3 On host join/identity/mapped attr report with usable ieee+short, update that IEEE’s short, clear collisions on other Zigbee records, persist when changed — verify: two devices cannot keep the same stored short after the second report

## 2. SPI device sync trailer

- [x] 2.1 Extend `SPI_DEVICE_SYNC_ENTRY_LEN` by 2 LE short bytes after type; update pack/unpack and length asserts — verify: pack length ≤ `SPI_MAX_PAYLOAD` and legacy unpack without trailer yields short `0`
- [x] 2.2 Confirm host enqueue and slave dump paths use the new length and still accept older shorter frames — verify: mixed-length unpack fixtures or log-free sync of a device with and without short

## 3. Slave seed + resolve

- [x] 3.1 On registered-device upsert/sync apply, call `rememberShortIeee` when synced short is usable — verify: after sync, `findByShortAddr` / resolve returns that IEEE before any stack neighbor entry
- [x] 3.2 Ensure short-only attr/zone reports use the seeded map when stack IEEE lookup fails, for all kinds on the shared report path — verify: simulated/report path yields non-zero IEEE on SPI to host for a seeded short
- [x] 3.3 When resolve learns a new short for an IEEE, update bound map and (via existing host report path) drive host short refresh — verify: after identity with a new short, later reports use the new mapping

## 4. Version + integration smoke

- [x] 4.1 Bump `FIRMWARE_VERSION` build on apply (after propose `3.9.21`) when source lands; smoke host+slave build — verify: version string in Status / boot log
- [x] 4.2 After flash, reboot coordinator with at least one sleepy registered sensor that has a stored `nwk`; confirm attribute reports show real IEEE/name (no continuous `Unmapped device state` for that short) — verify: log + Devices/MQTT state update without re-pair

## Workflow follow-up

- Archive the change after review when the human asks for `/opsx-archive`.
