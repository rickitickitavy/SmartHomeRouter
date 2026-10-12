# Design

## Context

See proposal.md — Why. Today listeners store `ieee` + optional `cluster`; conditions store ieee/channel/optional cluster + string compare; device actions store ieee/channel/`payload`. Status traffic already publishes interpreted strings (ON/OFF, window covering move+position, battery, measurement text) via `ZigbeeCoordinator` / SPI proxy. The console device-type catalog in `ZigbeeDeviceType.h` / `index.html` classifies devices but the automation editor does not use it for parameter pickers. Trigger Remove uses `btn-inline`; Actions Remove does not always share the same compact toolbar treatment the operator expects for consistency.

## Goals / Non-Goals

**Goals:**
- One shared “device binding” UI pattern for Trigger, Condition, and device Actions (device → channel if multi → known param | CUSTOM).
- Persist typed bindings so evaluation matches specific events; CUSTOM condition uses numeric VAL; CUSTOM action uses cluster/command/value.
- Keep older automations loadable with previous matching semantics when typed fields are absent.

**Non-Goals:**
- Reworking Zigbee attribute reporting or inventing new device types beyond catalog mapping.
- Changing automation-target actions (ENABLE/DISABLE/timer ops).
- Full ZCL attribute browser for every cluster; only a curated known list + CUSTOM.

## Decisions

### 1. Stable known-parameter keys
Persist a string `param` (e.g. `status`, `position`, `battery`, `sensorValue`, `lightLevel`) plus optional `paramValue` for action payloads that need a number/enum. Map keys → cluster/attrId (and for actions → command/payload) in one shared table used by UI and host.

**Alternative considered:** Persist only cluster/attrId from the UI with no key — rejected because Status vs Position on window covering must stay operator-readable and survive catalog tweaks.

### 2. CUSTOM fields
- Listen / Condition CUSTOM: `cluster` + `attrId` (uint16).
- Action CUSTOM: `cluster` + `command` + `value` (string or integer as already used by command path).
Condition CUSTOM compare value is numeric text; engine parses live status for that attr as VAL (from structured status when available, else from measurement/raw encoding already on the wire).

**Alternative considered:** Encode everything in free-form payload JSON — rejected; breaks list/export clarity and validation.

### 3. Channel visibility
UI derives channel count from the registered device’s `channels` field (same rules as Devices/Manual: hide when count is 1; show when ≥2 or special multi). Persist channel as today; default 1 when hidden.

### 4. Stricter listen matching
Extend `AutomationListener` with optional `param` and `attrId` (or derive attrId from `param`). On status change, pass enough identity (cluster + attr when known) into `AutomationEngine::onStatusChanged` / evaluateMatching so a Position listener ignores Move Status updates. When only legacy `cluster` is set, keep current cluster filter; when neither param nor cluster, keep any-status behavior.

### 5. Custom device commands
Prefer extending the device-action JSON with `cluster`/`command`/`value` and teaching the existing device-command path (or a thin wrapper) to accept a structured custom command when payload is empty/custom. Map known actions to today’s string payloads (ON, OFF, OPEN, CLOSE, STOP, brightness, etc.) so consoles and automations stay aligned.

**Alternative considered:** Only UI sugar that writes opaque payload strings — weaker validation and harder CUSTOM condition symmetry.

### 6. Remove button sizing
Normalize Trigger/Condition/Actions Remove (and Up/Down if needed) to `btn btn-secondary btn-inline` inside the shared automation editor toolbar/row pattern so heights match.

### 7. Known catalog (initial mapping)
| Device class | Params |
| --- | --- |
| Window covering | `status` (STOPPED / MOVING UP / MOVING DOWN), `position` (0–100) |
| On/Off | `status` (ON / OFF) |
| Level / dimmer | `status` (ON / OFF), `lightLevel` (0–254 or %) |
| Measurement sensors | `sensorValue` (typed measurement text/number), `battery` when power-config present |
| IAS zone | `status` (host strings e.g. LEAK/DRY or alarm/clear) |
| Others | CUSTOM only until catalog grows |

Ground attr/command ids in existing `ZigbeeCluster.h` constants and ZCL (OnOff 0x0006/0x0000, Level 0x0008/0x0000, Window covering lift % / move status, Power config battery % remaining 0x0021, measurement MeasuredValue 0x0000).

## Risks / Trade-offs

- [Status messages lack attrId today] → Extend status-change callback or parse encoded state so listen can filter by attr; if only cluster+interpreted text is available, map known `param` to expected text prefixes/keys already emitted.
- [CUSTOM VAL parse fails on non-numeric status] → Treat comparison as false and log when `debug` is on.
- [Custom actions unsupported by current payload API] → Implement structured custom send or document encoding; reject save if custom fields incomplete.
- [Catalog drift vs real device] → Always offer CUSTOM; never block unknown types.

## Migration Plan

1. Ship firmware that reads new optional JSON fields with defaults (absent = legacy).
2. Editor writes typed fields for new edits; Export/Import round-trip them.
3. Rollback: older firmware ignores unknown keys; may not filter attr-specific listens until upgraded.

## Open Questions

- Exact percent vs 0–254 for Light level in the UI (follow whatever Manual command / console widgets already use).
- Whether multi-endpoint battery should bind to endpoint 1 by default when channel is hidden.
