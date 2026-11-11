## Context

Console On/Off widgets and the Devices status column read host per-endpoint `status[].state`. Only exact `ON` counts as on; any other string (including converted numbers) shows as OFF / unknown. A kitchen Tuya On/Off light reported `ON`, then later `Basic cl=0x0000,attr=0x0001,val=0x53` (ZCL ApplicationVersion). The slave shaped non-identity Basic attrs onto the attribute-report path; the host `noteReportTelemetry` converted the value and called `setEndpointState`, wiping `ON`. The same report refreshed online. Mature stacks treat ApplicationVersion as genBasic metadata only.

## Goals / Non-Goals

**Goals:**
- Keep discrete channel status (`ON`/`OFF`/zone/covering tokens) stable when Basic ApplicationVersion or other non-status `cl=` reports arrive.
- Align slave Basic handling with identity-only semantics (manuf/model apply; no status forward).
- Host defense-in-depth for Basic, Power Config battery-as-`cl=`, and any converted `cl=` that would replace discrete status.

**Non-Goals:**
- Changing MQTT born / status-collect schedule.
- Persisting ApplicationVersion in the UI or devices.json.
- Changing how On/Off widgets interpret `ON` vs other strings.

## Decisions

1. **Slave: early-return entire Basic cluster** in `handleAttributeReport` after log/pulse; apply manuf/model identity only. Do not call `lightStateHandler` for ApplicationVersion or other Basic attrs.
2. **Host: reject Basic `cl=` for channel status** in `noteReportTelemetry` (return after optional reportable notify). Same for Power Config battery when delivered as `cl=` (match existing `BATTERY %u` early return).
3. **Host: discrete lock** — if cached state is discrete and the incoming converted `cl=` message is not discrete, do not `setEndpointState`; still allow reportable cache/notify for measurements.
4. **Scope is all device kinds** that share the endpoint status cache, not On/Off-only.

## Risks / Trade-offs

- [Risk] Measurement devices that incorrectly relied on Basic attrs as gauge status → Mitigation: Basic never was a valid gauge source; reportableValues still receive converted text if convert succeeds before Basic early return (host returns without status write).
- [Risk] Some `cl=` On/Off shaped as raw val without ON/OFF vocabulary → Mitigation: On/Off path on slave still emits `ON`/`OFF` strings; discrete lock only blocks non-discrete overwrites.

## Migration Plan

Flash host and slave together (`FIRMWARE_VERSION_HOST` / `FIRMWARE_VERSION_SLAVE` builds for this change). No settings migration. Devices already showing unknown may need a fresh On/Off report or operator toggle to restore `ON`/`OFF` in cache.

## Open Questions

- None for this fix.
