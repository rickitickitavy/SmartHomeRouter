# Proposal

## Why

Operators cannot view or change Zigbee device configuration attributes (power-on behavior, switch type, indicator mode, and similar) from the console. Manufacturer is only a join-time string and is not a durable enum on the registered device, so the UI cannot decide which settings apply. Near-term need is Configure for common On/Off relays and related types from the brands that dominate DIY Zigbee fleets.

## What Changes

- Research-backed **manufacturer enum** for common Zigbee end-device brands: **Tuya**, **Aqara** (Lumi/Xiaomi), **Philips Hue** (Signify), **IKEA**, **Sonoff** (eWeLink), plus **LEDVANCE/OSRAM**, **Innr**, **Gledopto**, **Sengled**, **Third Reality**, **Heiman**, **Legrand**, **Schneider**, **Namron**, **Develco**, and **Unknown** for everything else.
- Persist that enum on each registered **device entity** (not a free-form manufacturer string as the source of truth). Map join `manufacturer` / Zigbee manufacturerName into the enum at registration/join classification; keep displaying a human label.
- Add a firmware **configuration catalog** keyed by manufacturer + classified device type listing **configurable** attributes only (exclude runtime status, position, on/off state, battery, RSSI, measurement values).
- Devices row **…** menu gains **Configure** when the device has at least one catalog entry. Opening it shows a dialog with a **grid** of those attributes, values **read live from the device** (not stored on the host record). **Save** writes **only changed** attributes back to the device, respecting each attribute’s type (bool / enum / integer / etc.).
- `FIRMWARE_VERSION` propose bump `3.6.06` → `3.6.07` (apply bumps again when source lands).

## Capabilities

### New Capabilities

- `device-configure`: Catalog of configurable Zigbee attributes by manufacturer and device type; live read into the Configure dialog; write-changed-only save; attribute type rendering/validation. Config values are not persisted on the device registry record.

### Modified Capabilities

- `device-registry`: Registered devices store a manufacturer enum (persisted settings field with full life cycle: save, list, export/import, reboot).
- `web-console`: Devices row **…** menu offers Configure when applicable; Configure dialog UI (grid, typed controls, live load, save changed only); role gates aligned with device control/edit.

## Impact

- Host: `DeviceTopicEntry` + devices.json / list JSON; manufacturer mapping helpers; Configure HTTP APIs (schema + live get/set); `WebConsole` + `data/index.html` / CSS.
- Slave: existing ZCL read/write SPI paths reused; catalog lives on host (or shared header) so the dialog can target cluster/attr/datatype.
- No change to fragile OTA/SPI-OTA zones.
- MQTT-transport devices: no live Zigbee Configure in v1 (no Configure menu item unless a future catalog defines non-Zigbee config).
