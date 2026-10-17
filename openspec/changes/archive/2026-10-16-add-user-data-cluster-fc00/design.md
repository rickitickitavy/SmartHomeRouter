# Design

## Context

See `proposal.md` for motivation. Today device types are catalogued in `ZigbeeDeviceType.h` and mirrored in `data/index.html` `deviceTypeCatalog`. Registered records live in `DeviceTopicEntry` / `devices.json` with a single `zigbeeType` byte and no per-device field schema. Widget/automation param pickers use fixed known-parameter lists per type (`knownParamsForDevice` style helpers). MQTT and Zigbee already share a `cl=…,attr=…,val=…` vocabulary for many paths; user asked MQTT user-data bodies to be brace-list only.

Decided with the operator: attr always `0x0001`; types `int`/`float`/`bool`/`string`; null as `NULL` or empty `,,` slot.

## Goals / Non-Goals

**Goals:**

- Catalog type `userData` → cluster `0xFC00`.
- Persist ordered `userDataFields` on the host with the device; edit only in the parameter dialog; appear in lists after Save.
- Parse Zigbee `cl=0xFC00,attr=0x0001,val={…}` and MQTT `{…}` into named slots; expose those names in widget/automation param dropdowns.

**Non-Goals:**

- Defining a public manufacturer ZCL profile beyond this gateway’s convention.
- Slave EEPROM storage of the field schema (host-owned settings; slave keeps type id if already dumped).
- Arbitrary multi-attribute ids under `0xFC00` (only `0x0001` for the ordered list).

## Decisions

### 1. JSON shape

```json
{
  "type": "userData",
  "userDataFields": [
    { "nullable": false, "name": "count", "type": "int" },
    { "nullable": true, "name": "label", "type": "string" }
  ]
}
```

Store on host `devices.json` / list APIs. Cap field count modestly (e.g. 16) to protect heap; reject empty names and duplicate names on Save.

**Alternatives:** encode schema in a single string blob (harder to validate); one attr id per field (rejected by operator).

### 2. Catalog / type id

Add `userData` to firmware catalog with cluster `0xFC00` and a new stable `typeId` (next free id in `ZigbeeDeviceTypeInfo`). Map `zigbeeDeviceTypeFromCluster(0xFC00)` → that type. UI catalog label **User data**. Extend the classic enum only if radio paths require it; otherwise keep extended catalog pattern used for measurement types.

### 3. Runtime values

Keep last parsed slots as runtime telemetry on the host (like other status), keyed by field name or index — not written into persisted `userDataFields`. Devices table status cell MAY show a short joined summary later; minimum for this change is parse + binding display of the selected field.

### 4. Parsing

Shared brace-list parser: split top-level commas respecting quotes; trim; map index → field; `NULL`/empty → null if `nullable`, else treat as invalid/empty per field rules. Bool: `true`/`false`/`on`/`off`. Zigbee path: require `cl=0xFC00` and `attr=0x0001` (or accept hex case variants already used). MQTT path: body is `{…}` only (optional whitespace).

### 5. UI table

In parameter dialog when type is `userData` (MQTT editable type, or zigbee readonly showing User data): render table columns nullable | name | type; row actions Remove / Up / Down; Add appends draft row. Draft only until Save (no-preadd rule). Hide section for other types.

### 6. Widget / automation params

Extend `knownParamsForDevice` (and console binding param fill) so `userData` yields one option per field name (id = name, label = name) plus `custom`. Binding stores `param` as that name. View mode reads the corresponding runtime slot.

### 7. SPI / slave

Host persists schema. Slave dump continues to carry type byte if the catalog id fits existing SPI fields; schema does not need to cross SPI for v1. Zigbee inbound reports still arrive as text/`cl` payloads the host already routes.

## Risks / Trade-offs

- [Heap growth from many fields] → Cap count; bounded name length.
- [Quoted strings with commas] → Quote-aware splitter.
- [Legacy type enum only four values in older specs] → Specs updated; load unknown type strings carefully; old firmware without `userData` ignores new JSON field.

## Migration Plan

Existing devices unchanged. New optional `userDataFields` defaults to `[]`. Export/import include the array. Rollback: ignore unknown type / extra JSON keys on older builds.

## Open Questions

None blocking; field-count and name-length caps can be chosen at implement time within the heap budget used for other device strings.
