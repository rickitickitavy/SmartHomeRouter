# Proposal

## Why

Operators want Wi‑Fi/MQTT devices that speak the same wire vocabulary as Zigbee attribute traffic (`cl=…,attr=…,val=…,ep=…`) so consoles, automations, Manual command, and multi-endpoint devices stay consistent without inventing a second payload dialect. Today MQTT devices use plain `ON`/`OFF` (and channel topic suffixes / `ch-<ep>##`), and multi-channel MQTT commands already lose the endpoint on publish.

Separately, multi-metric sensors (e.g. Zbeacon TH01/TS0202: temperature, humidity, battery on one endpoint) have no durable place for last values keyed by `(cluster, attribute)`, and widgets cannot pick which metric to show without overloading CHANNELS (endpoints).

## What Changes

- Add a persisted per-device **Zigbee protocol** flag for `mqtt` transport only (default off).
- Device parameters dialog: checkbox **Zigbee protocol** immediately after **DEVICE SOURCE**, visible only for MQTT devices.
- When the flag is on:
  - Host uses the stored **STATE** topic as the inbound status topic and **COMMAND** as the outbound command topic, **exactly as configured** (no `/<ep>` suffix and no `ch-<ep>##` wrapping), regardless of `channels`.
  - Inbound status and outbound commands use Zigbee-style bodies (`cl` / `attr` / `val` / `ep`, plus `cl`/`cmd` where cluster commands apply).
  - Host encodes type shortcuts (`ON`/`OFF`/`TOGGLE`, covering `OPEN`/`CLOSE`/`STOP`, etc.) into that form when publishing to the command topic, including `ep` from the chosen channel.
  - Host parses inbound bodies for `ep=` (default `1`) and updates console/automation status from those reports.
- **BREAKING** only for devices that opt in: existing plain-text / suffix / parse-mode MQTT contracts no longer apply while the flag is on. Flag off keeps today’s MQTT behavior.
- Availability topic behavior is unchanged.
- Flag is host-only (MQTT path); slave Zigbee registry sync does not need it.

### Reportable data + Zbeacon

- Add manufacturer **Zbeacon** (json `zbeacon`), detected from join string like other vendors (Tuya, …).
- Each device (Zigbee and MQTT) MAY store a **reportable data** list: `(cluster, attribute, type)` with `type` in `int` / `float` / `bool` / `string`. Max **16** rows; memory allocated for the used count only.
- Auto-fill on **create** from manufacturer seed catalog **plus** discovered device class / known measurement clusters; list remains editable afterward.
- Runtime last values keyed by `(endpoint, cluster, attribute)` with ZCL→type conversion via a built-in table for known pairs (temp/humidity hundredths, battery half‑%); unknown pairs use raw numeric/string with no scale.
- Widget (and similar) bindings: **Reported data** picker **in addition to** Channel (CHANNELS stay endpoints).
- If an inbound `cl`/`attr` is not in the list: **log** the report even when device DEBUG is off; do not auto-append.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `device-registry`: persist `zigbeeProtocol` on `mqtt` records; persist `reportableData[]` on all transports; add manufacturer `zbeacon`.
- `mqtt-device-topics`: when `zigbeeProtocol` is on, disable channel topic/payload mapping; use Zigbee-style bodies on exact state/command topics.
- `wifi-mqtt-devices`: MQTT devices with the flag listen/publish in Zigbee-protocol form; still no Zigbee radio for those records.
- `web-console`: Zigbee protocol checkbox; reportable-data editor; widget Reported-data picker next to Channel; Zbeacon label.
- `device-custom-commands`: MQTT execute encoding uses Zigbee `cmd=` / `cmd=,val=` when the flag is on.

## Impact

- Host: `DeviceTopicEntry` + `devices.json` / device APIs; MQTT subscribe/publish mapping; Wi‑Fi state ingest; command enqueue/publish; Manual command / type-action / automation / custom-command paths for `mqtt` devices; reportable runtime store and conversion; manufacturer enum.
- Slave: attribute/measurement reports identify `cl`/`attr` so the host can route multi-metric values.
- Web console: device parameter dialog and console widget binding (`data/index.html`).
- Specs: deltas under the capabilities above.
