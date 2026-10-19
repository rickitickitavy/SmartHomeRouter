# Design

## Context

See proposal.md — Why.

Observed limits today:
- `kUserDataFieldMaxCount = 16` in `include/UserDataPayload.h` (brace-slot parse / field arrays).
- Console: `normalizeUserDataFields` and `addUserDataFieldRow` hard-code `16`.
- Schema JSON is copied with a **768**-byte bound (`duplicateBoundedString(..., 768)` in `DeviceTopicMap` / `WebConsole` / `main`). At 24 fields with names up to 32 chars, serialized JSON can exceed 768 bytes and silently truncate.

## Goals / Non-Goals

**Goals:**
- Single shared max of 24 for firmware and UI.
- Persist and reload 24-field schemas without truncation.
- Brace-list parse/accept up to 24 slots for attr `0x0001`.

**Non-Goals:**
- Changing field name/type/nullable rules.
- Raising MQTT/Zigbee body size limits beyond what 24 slots need.
- UI redesign of the fields table.

## Decisions

1. **Raise `kUserDataFieldMaxCount` to 24**  
   One constant drives firmware slot arrays and should be the source of truth. Console mirrors the same number (literal `24` or a small shared comment/constant in JS).  
   *Alternative:* only raise UI — rejected; firmware would drop slots 17–24.

2. **Raise `userDataFieldsJson` bound**  
   Introduce a named bound (e.g. `kUserDataFieldsJsonMaxLen`) sized for 24 max-length rows (recommend **2048** bytes) and use it everywhere `768` is used for `userDataFieldsJson`.  
   *Alternative:* leave 768 — rejected; long names at 24 fields would corrupt persistence.

3. **Reject/truncate at 24 consistently**  
   UI blocks Add past 24; normalize caps at 24; firmware parse uses max 24 slots. Prefer UI/block + normalize cap over silent mid-JSON cut from the old byte bound.

## Risks / Trade-offs

- **[Risk] Larger per-device RAM for schema strings** → Mitigation: only `userData` devices allocate; 2048 vs 768 is modest.
- **[Risk] Stack `String slots[24]` in parse helpers** → Mitigation: already used for 16; 24 remains small on ESP32; keep helper stack usage reviewed if nested deeply.
- **[Risk] Existing devices with 16 fields unchanged** → No migration; only max increases.

## Migration Plan

- Host firmware + `data/index.html` together.
- Rollback: prior image restores max 16 (schemas with >16 fields would need re-edit after rollback).

## Open Questions

- None; 24 is the explicit product target.
