# Design

## Context

See proposal.md — Why.

Observed today:
- `DeviceTopicEntry` already carries heap arrays for `userDataFields` (typed name/type rows) with LittleFS JSON round-trip — a close pattern for custom commands.
- Automation actions already have ZCL `hasCustomCommand` + `customCluster` / `customCommand` / `customValue`. That is **not** the same as named device custom commands; they must not collide.
- Place bindings (`ConsoleBinding`) have no click-action field; view mode sends On/Off / covering / etc. directly.
- Zigbee no-value bodies in this gateway omit `val=` (`cl=…,cmd=…`). Named custom commands follow the same omit rule: `cmd=${name}` vs `cmd=${name},val=${value}`.

## Goals / Non-Goals

**Goals:**
- Persist up to 8 named commands per device with optional typed parameter metadata.
- One execute helper that builds MQTT vs Zigbee payloads from transport + command def + value.
- Surface those commands in automation actions and optional Place binding click actions (full automation action editor).
- Keep Add → memory → Save (no pre-add) for commands and for enabling a click action draft.

**Non-Goals:**
- Slave storage or SPI sync of custom commands.
- Changing ZCL CUSTOM automation actions.
- Multi-parameter commands or command display names separate from the wire name.
- Redesigning Place layouts beyond making a configured binding an action button.

## Decisions

1. **Storage shape on the device**  
   Persist `customCommands` as a JSON array on each device record, e.g. `{ "name", "hasParam", "paramType" }` where `paramType` is `int`|`bool`|`float`|`string` when `hasParam` is true. Cap at 8; name ≤11; mirror constants next to userData limits (new `DeviceCustomCommand*` constants). Clone/free like `userDataFields`.  
   *Alternative:* pack into reserved EEPROM — rejected; devices already live in LittleFS JSON.

2. **Distinguish from ZCL CUSTOM in automation actions**  
   Introduce an explicit action branch for device custom commands (e.g. `param` sentinel `deviceCmd` plus `command` name string, or dedicated `deviceCustomName` field). Keep existing `hasCustomCommand` strictly for ZCL cluster/command id. Engine: if device-custom branch, look up name on the target device and call the shared execute helper; else keep today’s paths.  
   *Alternative:* overload `hasCustomCommand` — rejected; would break ZCL CUSTOM and confuse UI.

3. **Shared execute helper**  
   One host function: `(entry, commandName, valueTextOrNull, channel) →` publish MQTT payload or issue Zigbee body via existing `deviceCommand` / Manual-command path. Bool → `ON`/`OFF`; float → `%.2f`.  
   *Alternative:* duplicate format strings in engine and console — rejected.

4. **Console binding click action**  
   Optional nested object on the binding (e.g. `clickAction`) parsed/serialized with the same helpers as one `AutomationAction` (or a thin shared DTO). When set, view-mode UI renders a button and POSTs an execute-action API (or reuses automation action run endpoint if one exists; otherwise a small `/api/consoles/...` or devices command wrapper that accepts the action JSON). Edit mode ignores activation.  
   *Alternative:* only allow binding to that device’s custom commands — rejected; operator chose full automation action editor (3A).

5. **UI reuse**  
   Extract or share the automation action row editor for (a) automation actions, (b) binding click-action editor. Device dialog gets a small commands table (Add/Edit/Remove) on the draft, committed only on device Save.  
   *Alternative:* separate one-off editors — rejected; behavior must stay identical.

6. **Zigbee named `cmd=` vs ZCL parse**  
   Existing `parseZclCommandBody` expects hex `cl=`/`cmd=`. Named string commands need a dedicated branch before or beside ZCL parse: if body matches `cmd=<name>` with optional `val=` and `name` is not a hex id / matches a registered custom command, execute as custom string command (or always treat non-hex cmd names as custom payloads to the radio/MQTT bridge as opaque Manual bodies). Prefer: build body in the helper and send through the same Manual-command entry used for arbitrary bodies so Zigbee path stays consistent with today’s opaque command posting where applicable.  
   *Alternative:* force JSON Zigbee2MQTT style — rejected; this product uses `cmd=`/`val=` bodies.

## Risks / Trade-offs

- **[Risk] Name collision with ZCL CUSTOM UI** → Mitigation: separate dropdown entries and action fields; never set `hasCustomCommand` for device-named commands.
- **[Risk] Larger device JSON / RAM** → Mitigation: hard cap 8; short names; free lists on replace.
- **[Risk] Click action targeting another device surprises operators** → Mitigation: editor is explicit (same as automations); document in UI label “Click action”.
- **[Risk] Float formatting locale** → Mitigation: always format with `.` and two digits in firmware, not browser locale alone.
- **[Risk] Zigbee stack ignores unknown string `cmd=`** → Mitigation: still deliver body on command path as specified; device-side handling is out of scope.

## Migration Plan

- Older devices/consoles/automations without new fields load with empty commands / no click action / unchanged actions.
- Deploy host firmware + `data/index.html` + `data/user.html` together.
- Rollback: prior firmware ignores unknown JSON keys; re-flash drops new fields from effective behavior until re-saved on new firmware.

## Open Questions

- None that affect specs or task breakdown; API path for “run this action JSON from Place” can be chosen at apply time as either a dedicated endpoint or reuse of an existing command runner, as long as view-mode behavior matches the specs.
