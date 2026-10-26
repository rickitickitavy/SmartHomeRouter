# Design

## Context

See proposal.md — Why. Today: join carries manufacturer/model strings on the slave `BoundZigbeeDevice` / found list, but `DeviceTopicEntry` does not persist a manufacturer enum. Devices row **…** only lists type actions (ON/OFF/…); Manual command and SPI ZCL read/write already exist on host↔slave. Configurable settings are brand-specific (Tuya `0x8002` power-on, Aqara manu-specific clusters, standard ZCL `startUpOnOff` `0x4003`, etc.).

Research basis for brands (DIY Zigbee / Zigbee2MQTT ecosystem prevalence, not chipset market share): **Tuya**, **Aqara/Lumi/Xiaomi**, **Philips Hue**, **IKEA**, **Sonoff**, plus **LEDVANCE/OSRAM**, **Innr**, **Gledopto**, **Sengled**, **Third Reality**, **Heiman**, **Legrand**, **Schneider**, **Namron**, **Develco**, and **Unknown**.

## Goals / Non-Goals

**Goals:**
- Persisted manufacturer enum on the device entity with full settings life cycle.
- Host-side config catalog + Configure UI that live-reads and writes-changed-only.
- Typed editors matching catalog value types.
- Reuse existing ZCL read/write SPI commands.

**Non-Goals:**
- Full Zigbee2MQTT converter parity (thousands of models).
- Persisting config attribute values on the host device record.
- Configure for MQTT-transport devices in v1.
- Changing Zigbee mesh/routing or APS ACK behavior.
- Fragile OTA / slave-spi-ota paths.

## Decisions

### 1. Manufacturer enum + join string mapper
- Store `uint8_t manufacturer` on `DeviceTopicEntry` with JSON ids: `unknown`, `tuya`, `aqara`, `philipsHue`, `ikea`, `sonoff`, `ledvance`, `innr`, `gledopto`, `sengled`, `thirdReality`, `heiman`, `legrand`, `schneider`, `namron`, `develco`.
- Mapper from Zigbee manufacturerName / join string (case-insensitive / contains): `_TZ` / `Tuya` → tuya; `LUMI` / `Aqara` / `Xiaomi` → aqara; `Philips` / `Signify` → philipsHue; `IKEA` → ikea; `SONOFF` / `eWeLink` → sonoff; `LEDVANCE` / `OSRAM` → ledvance; `innr` → innr; `GLEDOPTO` → gledopto; `sengled` → sengled; `Third Reality` → thirdReality; `Heiman` → heiman; `Legrand` / `Netatmo` → legrand; `Schneider` → schneider; `Namron` → namron; `Develco` → develco; else unknown.
- **Source of join strings:** After join, the slave queues ZCL Basic reads for ManufacturerName (`0x0004`) and ModelIdentifier (`0x0005`), copies the char-string replies onto `BoundZigbeeDevice`, and re-emits the SPI join so the host found list / enum mapper see real values (type classification still comes from ZDO Active EP / Simple Desc).
- **Persisted model:** Host `DeviceTopicEntry` stores the Basic model string (bounded) for catalog matching; manufacturer remains the enum. Catalog rows stay `static const` in flash with optional `modelPrefix` (nullptr = any model for that brand+type). No heap-allocated catalog.
- **Alternative considered:** Keep free-form string only — rejected; user asked for enum stored on the entity.
- **Alternative:** Embed full Zigbee2MQTT converter DB — rejected; flash budget; grow curated flash tables from Z2M/ZHA research instead.

### 2. Catalog as compile-time tables
- Shared header (e.g. `DeviceConfigCatalog.h`) listing entries: `{ manufacturer, zigbeeType, configId, label, valueType, enumChoices[], endpoint, cluster, attr, zclDataType, manuSpecific }`.
- Query: `listConfigAttrs(manufacturer, type)` → pointer/count.
- **Alternative:** LittleFS JSON catalog — more flexible but heavier and not required for v1.

### 3. Catalog content (researched from Z2M/ZHA; flash-only)

For **every** project manufacturer (Tuya, Aqara, Sonoff, Philips Hue, IKEA, LEDVANCE, Innr, Gledopto, Sengled, Third Reality, Heiman, Legrand, Schneider, Namron, Develco), the flash catalog covers these device kinds with ZCL/Z2M-documented **config** attrs (not runtime status/position/battery):

| Kind (`zigbeeType` id) | Typical configurable attrs |
|------------------------|----------------------------|
| onOff (1) | power_on_behavior (`0x4003` or brand-specific); Tuya switch_type / indicator; Aqara operation_mode + FCC0 power-on; Sonoff network_indicator |
| levelControl (11) | power_on + `startUpCurrentLevel` `0x4000` |
| colorControl (40) | power_on + startup level + `startUpColorTemperature` `0x4010` |
| windowCovering (3) | mode `0x0017`; Tuya motor_reversal `0xF002`, calibration `0xF001`, motor_mode `0x8000`, indicator/switch_type |
| shadeConfiguration (33) | shade mode |
| doorLock (34) | operating_mode, sound_volume |
| thermostat (36) | local_temperature_calibration `0x0010` (int8) |
| thermostatUi (39) | keypad_lockout |
| iasZone (2) | zone sensitivity |
| fanControl (37) | fan_mode_sequence |
| ballast (41) | min/max level |
| occupancy (48) | occupancy sensitivity |

Unknown manufacturer stays empty. BACnet/tunnel/SE clusters are not catalogued (not DIY end-device config). Exact cluster/attr constants live in `DeviceConfigCatalog.h`.

### 4. APIs
- `GET /api/devices/:ieee/configure` — returns `{ attrs: [ { id, label, type, choices?, value|null, error? } ] }` after live reads. Requires `controlDevices` / admin.
- `POST /api/devices/:ieee/configure` — body `{ changes: [ { id, value } ] }` writes only listed ids that exist in catalog; host compares optional client baseline or trusts client-sent changes only for ids present. Prefer server-side: client sends full edited map + original baseline hashes, **or** client sends only changed `{id,value}` (matches product: Save CHANGED). Spec: client sends only changed entries; host writes those.
- Role: same as **…** actions → `controlDevices`.

### 5. UI
- Extend `openDeviceActionsMenu` / `attachDeviceRowActions` so **…** appears when type actions **or** Configure applies.
- Configure dialog: CSS grid of label | control; Save / Cancel; loading and error plates. Follow existing console dialog patterns and enabled-controls-not-gray.
- Do not inject Configure into parameter edit dialog (separate flow).

### 6. Read/write path
- Host loops catalog entries → `ZigbeeSpiProxy::readAttribute` / `writeAttribute` (existing).
- Dialog open may take several round-trips; show per-row pending/unavailable rather than blocking forever; overall timeout returns partial results with errors.

### 7. Firmware version
- Propose: `3.6.07`. Apply bumps build again when source lands.

## Risks / Trade-offs

- [Wrong attr mapping for a Tuya sub-OEM] → Mitigation: catalog scoped to common attrs; failed reads show unavailable; expand catalog later per modelId if needed.
- [Sleepy devices miss reads] → Mitigation: Configure is aimed at mains On/Off; show timeout error; operator can retry.
- [… menu previously hidden when no type actions] → Mitigation: show **…** when Configure alone applies.
- [Flash size from large catalogs] → Mitigation: keep v1 tables small; Unknown empty.

## Migration Plan

- Existing devices.json without `manufacturer` → load as `unknown`; optional background remap if join string still available on slave dump (best-effort); operator can re-pair/re-save to refresh.
- Rollback: older firmware ignores unknown JSON key.

## Open Questions

None that block apply; model-level catalog expansion can follow after v1.
