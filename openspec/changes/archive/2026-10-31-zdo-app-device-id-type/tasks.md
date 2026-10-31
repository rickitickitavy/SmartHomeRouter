# Tasks

## 1. Application Device ID → type map

- [x] 1.1 Expand `zigbeeDeviceTypeFromHaDeviceId` (or a dedicated table next to it) for HA/ZHA/ZLO Application Device IDs listed in design/proposal (On/Off family, plugs, lights, covering, thermostat, temperature sensor, IAS Zone, etc.) and verify fixture coverage for representative IDs including `0x0051`, `0x0100`, `0x0302`, `0x0402`, `0x0202`
- [x] 1.2 Document unmapped → `unknown` behavior in a short code comment near the map and verify an unlisted ID (e.g. `0x0008`) returns unknown in the fixture

## 2. Slave classification path

- [x] 2.1 Change `ZigbeeCoordinator::onSimpleDescriptor` so type comes only from `app_device_id` mapping (no cluster-list classify/merge into type); keep non-type side effects such as power-config detection and verify build of `esp32-c6-slave`
- [x] 2.2 Ensure `applySavedDeviceClass` still runs before any new classification for registered IEEEs and verify a registered `onOff` is not replaced when a later Simple Descriptor would map differently
- [x] 2.3 For multi-endpoint probes, use the first usable endpoint Simple Descriptor that yields a mapped Device ID (else unknown) and verify join emit still carries that type for unregistered devices

## 3. Host registry freeze

- [x] 3.1 Remove host paths that update a **registered** Zigbee `zigbeeType` from later joins or identity (including former “unknown fills in later”) and verify registered `unknown` stays `unknown` after a classifying join
- [x] 3.2 Keep found-list type from join Application Device ID classification only (manuf/model must not set type) and verify found JSON type matches Device ID map
- [x] 3.3 Confirm first Save from found still copies found type once into the registry and verify subsequent joins do not change it

## 4. Configure vs type split

- [x] 4.1 Audit Basic identity / join handling so manuf+model never assign `zigbeeType`; verify Configure still resolves catalog by manufacturer, stored type, and model prefix
- [x] 4.2 Add or adjust a host/slave fixture that asserts Device ID sets type while a model string alone does not and verify fixture passes at boot

## 5. Integration check

- [x] 5.1 Bump already at propose (`3.9.29`); after code changes bump again per apply rules if source is edited in apply, build host+slave, and verify classifier fixtures and a manual new-pair vs already-paired regression checklist

## Workflow follow-up

- Archive the change after apply and review are complete.
- Sync delta specs into main specs when archiving or via `/opsx-sync` if needed earlier.
