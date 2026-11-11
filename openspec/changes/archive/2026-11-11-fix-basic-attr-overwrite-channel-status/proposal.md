## Why

Zigbee On/Off devices (including Tuya lights such as `House-kitchen-far` / `House-kitchen-near`) correctly report `ON`, then later emit ZCL Basic **ApplicationVersion** (`cl=0x0000,attr=0x0001`). The slave forwarded that as a shaped `cl=/attr=/val=` attribute report; the host converted it (e.g. `0x53` → `83.0`) and **replaced** the endpoint’s cached `ON`. Console widgets then looked OFF and the Devices table showed **unknown** status while the same packet refreshed `lastSeen` so the device stayed **online**. ApplicationVersion is firmware identity metadata (ZCL / Tuya OTA), not actuator state—Zigbee2MQTT and ZHA keep it off On/Off state. This change stops that corruption for all device kinds that use discrete channel status.

## What Changes

- Slave MUST NOT forward Basic-cluster attribute reports (including ApplicationVersion) onto the channel-status / `lightStateHandler` path. ManufacturerName / ModelIdentifier MUST still update identity as today; other Basic attrs MAY be logged only.
- Host MUST NOT write Basic `cl=` reports into per-endpoint channel status.
- Host MUST NOT let converted `cl=/attr=` measurement (or battery-as-`cl=`) text replace an existing discrete channel state (`ON`/`OFF`/`LEAK`/`DRY`/covering move tokens). Battery via `BATTERY <percent>` already obeyed this; `cl=` Power Config battery MUST match.
- Host `FIRMWARE_VERSION_HOST` build bumps for this change; slave `FIRMWARE_VERSION_SLAVE` build bumps because slave Basic handling changes. Flash host and slave together.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `host-slave-spi`: channel status cache must ignore Basic ApplicationVersion / identity metadata and must not let converted `cl=` reports overwrite discrete On/Off/zone/covering status
- `zigbee-slave-radio`: Basic cluster reports stay identity/metadata (log + manuf/model apply); never shaped as channel status to the host

## Impact

Slave `ZigbeeCoordinator::handleAttributeReport`; host `ZigbeeSpiProxy::noteReportTelemetry`; firmware versions in `include/Defines.h`. Console UI needs no markup change—it already keys Off/unknown off non-`ON` status. Existing On/Off, battery, IAS, covering, and measurement report paths stay; only the status-overwrite bug is closed.
