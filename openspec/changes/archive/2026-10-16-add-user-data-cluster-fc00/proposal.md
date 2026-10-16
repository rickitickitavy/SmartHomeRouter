# Proposal

## Why

Operators need devices whose payload is an ordered list of typed user fields (manufacturer-style cluster `0xFC00`), not a fixed ZCL type like On/Off. Without a catalog type and per-device field schema, those values cannot be named, bound in widgets, or parsed consistently for MQTT vs Zigbee.

## What Changes

- Add classified device type **User data** mapped to cluster `0xFC00` (JSON id `userData`).
- Each `userData` device MUST persist an ordered field schema: nullable, logical name, and type (`int`, `float`, `bool`, `string`).
- Device parameter dialog MUST show an editable schema table (columns: nullable checkbox, name, type) with per-row Remove / Up / Down; schema is memory-only until Save.
- Wire value for attribute `0x0001` is an ordered brace list matching the schema: Zigbee `cl=0xFC00,attr=0x0001,val={…}`; MQTT devices publish/consume the brace list only (`{…}`). Null/empty slots use `NULL` or an empty place between commas (`,,`).
- Bool accepts `true`/`false` or `on`/`off` (case-insensitive).
- When binding a `userData` device in a console widget (and automation known-parameter pickers), the parameter control MUST be a dropdown of that device’s schema field names (plus existing CUSTOM where applicable).

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `device-registry`: Persist `userData` type and ordered `userDataFields` schema on registered devices through save, list, export/import, and reboot.
- `web-console`: User-data type in MQTT type picker; parameter-dialog schema table UI; known-parameter lists for user-data devices.
- `control-consoles`: Widget device bindings expose dropdown of user-data field names for `userData` devices.
- `mqtt-device-topics`: Parse and publish MQTT brace-list payloads for `userData` devices (no `cl`/`attr` wrapper on MQTT).

## Impact

- Firmware catalog: `ZigbeeDeviceType.h` / UI `deviceTypeCatalog` (`userData`, cluster `0xFC00`).
- Persistence: `DeviceTopicMap` / `devices.json` / device APIs gain `userDataFields` (or equivalent); SPI dump may carry type id only if fields stay host-only (design will decide).
- Web UI: `data/index.html` (and user console binding display if needed).
- MQTT inbound/outbound state path for `userData` devices.
- Firmware build bump for this propose: `FIRMWARE_VERSION` `3.4.48` → `3.4.49`.
