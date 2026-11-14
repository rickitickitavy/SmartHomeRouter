# Design

## Context

See proposal.md — Why. Today boot already restores history into PSRAM (`HistoryStore::restoreSnapshot`) and syncs series (`syncSeriesFromDevices`), including per-series `hasLast` / `lastFloat` (and siblings). Console and device-list live readings come from `ZigbeeSpiProxy` runtime caches (`reportableValues`, endpoint main-state), which stay empty until a real report. `noteReportableValue` also appends history; seeding must not reuse that full path blindly.

## Goals / Non-Goals

**Goals:**
- After successful snapshot restore + series sync, fill live caches from each series’ last sample when present.
- Same display surfaces as live reports (device list JSON, Place, simple-user).
- No automation status-path side effects; no extra history append from the seed itself.

**Non-Goals:**
- UI “stale / from history” badge.
- Persisting live telemetry into devices.json/EEPROM.
- Changing history snapshot format or retention.
- Slave firmware changes.
- Seeding when no snapshot restored / no last sample.

## Decisions

1. **Silent seed API on the live cache**  
   Add a host path that writes reportable (and historized main-state) cache slots **without** calling `statusValueChanged` / automation hooks and **without** calling `historyNote*`.  
   Alternative considered: call existing `noteMqttState` / report paths and suppress callbacks — rejected as easy to miss a side channel.

2. **Use restored series last values, not a full query scan**  
   Prefer `SeriesRuntime.hasLast` + last typed value after restore (already in RAM). Fall back to reading the newest page record only if `hasLast` is incomplete after restore.  
   Alternative: `query(..., maxPoints=1)` per series — heavier I/O and redundant when snapshot already restored last fields.

3. **When to run**  
   Once per boot, after `restoreSnapshot` success (or after sync if restore loaded samples) and `syncSeriesFromDevices`, before normal operation relies on empty caches. Skip entirely when no snapshot / empty store. Re-run is not required on later SD remounts for this change.

4. **Scope of series**  
   All historized series that map to console/device-list live readings (reportable attributes and device main-state channels with history enabled). Skip series with no last sample. Do not invent values for non-historized attributes.

5. **Main-state formatting**  
   Format seeded main-state text the same way live MQTT/Zigbee status strings are stored today (on/off, covering position, etc.) so existing console parsers keep working.

6. **Operator + simple-user**  
   Both pages already share live APIs; seeding the host cache covers both without page-specific JS (sync rule satisfied by host-side fix).

## Risks / Trade-offs

- **[Risk] Operators treat seeded values as “fresh”** → Mitigation: accepted for this change; no stale marker (non-goal). Real reports still overwrite.
- **[Risk] Seed path accidentally appends history or fires automations** → Mitigation: dedicated silent writer; tasks include explicit “no historyNote / no statusValueChanged” checks.
- **[Risk] Endpoint/channel mapping wrong for multi-endpoint devices** → Mitigation: use series key channel/endpoint conventions already used when noting history from live reports.
- **[Risk] Boot time grows with many series** → Mitigation: walk in-memory series once; no LittleFS/SD remount for seed.

## Migration Plan

- Ship with host build bump only.
- Existing snapshots already carry last samples; no snapshot version bump required if `hasLast` is restored today.
- Rollback: omit seed call; behavior returns to empty live cache until reports.

## Open Questions

- None that block specs/tasks (automation non-fire chosen by operator as option A).
