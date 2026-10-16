# Tasks

## 1. Catalog and persistence

- [x] 1.1 Add `userData` / cluster `0xFC00` to firmware device-type catalog and cluster→type mapping — verify type id/jsonId round-trip in catalog helpers
- [x] 1.2 Persist `userDataFields` on registered devices (load/save `devices.json`, list APIs, export/import); omit/empty for non-userData — verify Save → reload → reboot keeps order, nullable, name, type
- [x] 1.3 Allow MQTT devices to store type `userData` (extend type validation beyond the previous four) — verify MQTT save with `userData` survives list JSON

## 2. Parse and MQTT/Zigbee value path

- [x] 2.1 Implement quote-aware brace-list parse/format with `NULL` / `,,` null slots and bool `true`/`false`/`on`/`off` — verify unit-style cases or host log for `{1000,,\"ON\",23.5}`
- [x] 2.2 Map Zigbee `cl=0xFC00,attr=0x0001,val={…}` and MQTT `{…}` bodies for `userData` devices onto named runtime slots — verify MQTT state updates the selected field; Zigbee-style string updates the same

## 3. Web console device dialog

- [x] 3.1 Add User data to MQTT type select and UI catalog label; show User data fields table only when type is `userData` — verify On/Off hides the table; MQTT User data shows it
- [x] 3.2 Table columns nullable | name | type; row Remove / Up / Down; Add is draft-only until Save; Cancel discards — verify reorder+Save persists; Cancel after Add does not persist

## 4. Widget and automation pickers

- [x] 4.1 Known-parameter / console binding param dropdown lists `userDataFields` names in order plus CUSTOM — verify binding Save stores `param` as a field name and view mode shows that slot when live data exists

## 5. Integration check

- [ ] 5.1 Create MQTT `userData` device with three fields, publish `{1,,on}`, bind second/first field in a widget — verify null middle slot and bool/on mapping; widget shows the bound field without full page reload after live update
