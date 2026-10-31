# Design

## Context

See proposal.md — Why. Today `onSimpleDescriptor` merges `classifyZigbeeDeviceTypeFromClusterList(...)` with `zigbeeDeviceTypeFromHaDeviceId(app_device_id)`. Host `FoundDeviceList` / registry rules can still fill or (historically) overwrite types. Basic manuf/model are already read. Configure already keys on manufacturer, type, and optional model prefix (`DeviceConfigCatalog`).

## Goals / Non-Goals

**Goals:**

- New/unregistered devices: type = Application Device ID mapping only.
- Registered Zigbee devices: type immutable after Save (including `unknown`).
- Manuf + model: Configure catalog selection only.
- Expand `zigbeeDeviceTypeFromHaDeviceId` (or equivalent table) to HA/ZHA/ZLO IDs listed in the proposal research.

**Non-Goals:**

- Re-classifying existing `devices.json` rows automatically.
- Using clusters to set type (clusters may still be used for power-config detection, command paths, etc.).
- Building a per-model type override catalog (explicitly rejected).
- Changing MQTT operator-editable types.

## Decisions

1. **Type source = `app_device_id` only in `onSimpleDescriptor`**  
   Stop merging cluster classification into `mergeBoundDeviceType` for type. Keep reading clusters for side effects (e.g. `hasPowerConfig`) if needed.  
   *Alternative considered:* Cluster priority with Device ID fallback — rejected by product rule.

2. **Freeze registered types on host and slave**  
   Slave: `applySavedDeviceClass` wins before any new classification. Host: do not update registered `zigbeeType` from later joins (remove “unknown fills in later”). First Save from found list copies found type once.  
   *Alternative:* Allow unknown→known fill-in after Save — rejected (“already paired” includes frozen `unknown`).

3. **Map Device ID → existing catalog type ids**  
   Reuse `ZigbeeDeviceType` / catalog json ids (`onOff`, `iasZone`, `windowCovering`, `temperatureMeasurement`, `thermostat`, level/color ids as already in catalog). Unknown Device ID → `unknown`. Expand `zigbeeDeviceTypeFromHaDeviceId` systematically rather than inventing a parallel enum.

4. **Config stays manuf + type + model prefix**  
   No code path from Basic identity into `zigbeeType` for registered or found devices.

## Risks / Trade-offs

- [Risk] Some Tuya/OEM devices report wrong or generic Application Device ID → Mitigation: type may be `unknown` or coarse; operator keeps MQTT path for overrides; do not “fix” with clusters or model type overrides.
- [Risk] Multi-endpoint devices with different Device IDs per endpoint → Mitigation: keep current “one type per IEEE” policy; use first usable endpoint’s Simple Descriptor that yields a mapped ID (document in tasks); do not invent multi-type IEEE in this change.
- [Risk] Specs previously promised unknown fill-in → Mitigation: delta explicitly removes that; operators re-Add only if they need a new classification.

## Migration Plan

- Flash host+slave with this change.
- Existing registry types unchanged.
- New pairings use Device ID classification.
- Rollback: prior firmware restores cluster merge behavior; stored types remain valid.

## Open Questions

- None that block specs: multi-endpoint Device ID conflict uses first mapped usable endpoint (recorded as implementation detail in tasks).
