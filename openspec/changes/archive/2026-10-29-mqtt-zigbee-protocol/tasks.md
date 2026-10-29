# Tasks

## 1. Persistence and API

- [x] 1.1 Add `zigbeeProtocol` to `DeviceTopicEntry`, clone/clear/upsert, `listJson` / `listStoreJson` / `replaceFromJson`, and device create/update API parsing; verify create omit → false, save true → round-trips in list JSON and `devices.json` after reload
- [x] 1.2 Ensure export/restore and settings dump include `zigbeeProtocol` for `mqtt` only; verify Zigbee records omit the field and MQTT export/import restores the flag
- [x] 1.3 Add `reportableData[]` (max 16, dynamic alloc) to `DeviceTopicEntry` with clone/clear/upsert, list/store JSON, create/update API, export/restore; host-only (SPI sync does not wipe or require it)
- [x] 1.4 Add manufacturer `Zbeacon` (`zbeacon`) to enum/json/join mapper + UI labels; Zbeacon seed catalog for reportable rows

## 2. MQTT wire behavior

- [x] 2.1 When `zigbeeProtocol` is on, subscribe/publish only exact STATE/COMMAND topics (no suffix wildcards / no `ch-<ep>##`); verify with `channels` 0 and 2–16
- [x] 2.2 Implement outbound Zigbee-shaped encoding for Manual command, type actions, automations, and custom commands (`ep` included; pass-through if body already shaped); verify `ON` on channel 2 publishes `…,ep=2` to unsuffixed command topic and does not hit Zigbee radio
- [x] 2.3 Fix inbound STATE ingest so non-`userData` `cl=`/`attr=`/`val=` reports update endpoint status (and known on/off/covering vocabulary where applicable); verify `cl=0x0006,attr=0x0000,val=0x1,ep=2` appears in device status for ep 2
- [x] 2.4 With flag on, `userData` uses `cl=0xFC00` / `attr=0x0001` (brace list in `val=`); with flag off, bare brace lists remain

## 3. Web console

- [x] 3.1 Add **Zigbee protocol** checkbox immediately after **DEVICE SOURCE** for `mqtt` dialogs only (square-only toggle); load/save with the device; verify Zigbee dialogs hide it and MQTT create defaults unchecked
- [x] 3.2 Show the checkbox for every MQTT class including User data
- [x] 3.3 Reportable-data editor on device parameters (add/remove/reorder rows: cluster, attribute, type); load/save; create may prefill from seed
- [x] 3.4 Console widget: **Reported data** select additional to Channel when device has reportable rows; binding stores chosen cl/attr; display uses runtime values

## 4. Runtime reportable + seed

- [x] 4.1 Runtime last-value store keyed by `(ep, cl, attr)`; ZCL→type conversion table; expose in device list telemetry JSON
- [x] 4.2 On create: auto-fill reportable from manufacturer seed ∪ discovered type/clusters when body omits list
- [x] 4.3 Unknown `(cl, attr)` vs non-empty list: always log (ignore device DEBUG); do not auto-append
- [x] 4.4 Slave measurement reports include `cl`/`attr`/`val` (raw) so host can match rows; convert for status/reportable

## 5. Integration check

- [x] 5.1 End-to-end: MQTT device with flag on, channels 4 — command path uses exact COMMAND + `ep=`, status path uses exact STATE + `ep=`, flag-off sibling still uses today’s plain/suffix behavior; verify both from console Manual command and a broker publish
- [ ] 5.2 Zbeacon (or seeded) device with temp/humidity/battery rows: reports update distinct reportable values; widget can pick humidity while channel stays 1; unknown cluster logs with DEBUG off
