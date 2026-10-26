# Tasks

## 1. Firmware bump and manufacturer enum

- [x] 1.1 Bump `FIRMWARE_VERSION` build by 1 on apply (propose already set `3.6.07`) and verify `include/Defines.h` shows the apply build
- [x] 1.2 Add `ZigbeeManufacturer` (or `DeviceManufacturer`) enum + JSON id/label helpers + join-string mapper (`tuya`/`aqara`/`philipsHue`/`ikea`/`sonoff`/`unknown`) and verify unit-style asserts in `main.cpp` self-tests (or equivalent) for Tuya/`_TZ3000_`, LUMI→aqara, and unknown mapping
- [x] 1.3 Persist `manufacturer` on `DeviceTopicEntry` through create/update, `devices.json`, list/entry JSON, clone/export/import, and reboot load; verify a saved Tuya device still shows `"manufacturer":"tuya"` after reload and that missing key loads as `unknown`

## 2. Configuration catalog

- [x] 2.1 Add compile-time `DeviceConfigCatalog` with typed entries (id, label, value type, enum choices, endpoint policy, cluster, attr, ZCL data type, manu-specific flag) keyed by manufacturer + zigbee type; verify empty catalog for `unknown`+any type and non-empty for Tuya `onOff`
- [x] 2.2 Populate v1 rows from design.md (Tuya power-on/switch type/indicator; Aqara power-on/operation mode; Philips/IKEA startUpOnOff; Sonoff power-on/network indicator) and verify status/position/battery/RSSI ids are not present in any catalog row
- [x] 2.3 Expose a host helper `deviceHasConfigurableAttrs(entry)` used by API/UI and verify it is false for MQTT transport and true for Zigbee Tuya onOff

## 3. Configure APIs and Zigbee I/O

- [x] 3.1 Add `GET` Configure endpoint that live-reads each catalog attr via existing SPI ZCL read, returns typed values/errors, does not write device store; verify 401/403 for unauthorized and that a successful response values are absent from a subsequent devices list persistence dump
- [x] 3.2 Add `POST` Configure endpoint that accepts only changed `{ id, value }` entries, validates types/enums, writes via SPI ZCL write, and rejects invalid enum without writing; verify one-field change issues one write path and no-op body writes nothing
- [x] 3.3 Wire join/register path so new Zigbee saves set manufacturer from found-list manufacturer string and later parameter saves do not clobber a known manufacturer; verify found Tuya save stores `tuya`

## 4. Web console Configure UI

- [x] 4.1 Show Devices row **…** when type actions exist **or** Configure applies; add **Configure** menu item when catalog non-empty; verify IAS/unknown without catalog still omit Configure and Tuya onOff shows it
- [x] 4.2 Implement Configure dialog with attribute grid, typed controls (checkbox/select/number), loading and error states, Cancel discard; verify enum rows render selects and Cancel performs no write
- [x] 4.3 Implement Save that posts only changed attributes and shows failure without closing on error; verify editing one of two fields sends a single change id
- [x] 4.4 Gate Configure menu and APIs with `controlDevices`/`isAdmin` and verify simple user cannot open Configure

## 5. Integration check

- [x] 5.1 Smoke on host+slave: register a mains On/Off Zigbee device (or simulator path), open Configure, confirm live values load, change power-on behavior, Save, reopen and confirm new value; verify devices.json still has no config attribute values stored

## 6. Expand manufacturer set

- [x] 6.1 Add ten manufacturers (`ledvance`, `innr`, `gledopto`, `sengled`, `thirdReality`, `heiman`, `legrand`, `schneider`, `namron`, `develco`) with JSON/label/join mappers and verify fixtures for LEDVANCE/OSRAM/Innr/… mapping
- [x] 6.2 Add curated onOff `power_on_behavior` (`startUpOnOff`) catalog rows for those ten brands and verify each has a non-empty onOff catalog without runtime status ids
- [x] 6.3 Bump `FIRMWARE_VERSION` build and update change proposal/design/spec deltas to list the expanded enum

## 7. Found-table manufacturer column

- [x] 7.1 Add **Manufacturer** column to the Add-device search found table and bind it to the join `manufacturer` string from `/api/devices/found`; show **Model** alone in the Model column; verify a found row displays the raw join manufacturer text
- [x] 7.2 On join, slave reads ZCL Basic `ManufacturerName` (0x0004) and `ModelIdentifier` (0x0005), stores them on `BoundZigbeeDevice`, and re-emits the SPI join so found/register paths receive real strings; bump `FIRMWARE_VERSION` build
- [x] 7.3 Add a readonly **Manufacturer** field on the Zigbee device edit dialog after Device class (hidden for MQTT); show the human manufacturer label from the stored enum (or join string when adding from found)
- [x] 7.4 Persist Zigbee `model` on the device entity (LittleFS/JSON lifecycle); keep config catalog as `static const` flash tables with optional `modelPrefix` (no heap catalog)
- [x] 7.5 Add researched Tuya `windowCovering` config attrs (motor reversal, calibration, indicator, switch type from Z2M/ZHA TS130F) and levelControl `startUpOnOff` for Philips/IKEA; Configure uses manufacturer+type+model
