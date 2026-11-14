# Proposal

## Why

After a router reboot, console and device-list live readings stay empty until each sensor reports again. Sensors that report rarely leave operators looking at blank gauges and “absent” values for a long time, even when the host already restored those series from historical data on SD or LittleFS.

## What Changes

- After a successful history snapshot restore into PSRAM (LittleFS or SD path), the host seeds the **runtime live-value cache** from each series’ **last stored sample** when one exists.
- Seeded values appear in the same places as live reports: registered-device list JSON (`reportableValues` / status where historized), Place and simple-user console widgets.
- Seeding is **display / live-API only**: it MUST NOT run automation status-path evaluation, MUST NOT enqueue actions, and MUST NOT be treated as a new radio/MQTT report.
- A later real report still overwrites the seeded value and follows normal history/automation rules.
- No new persisted settings entity; still not writing live telemetry into `devices.json` / EEPROM.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `historical-data`: After boot restore, expose last sample into the host live-value cache for historized series that have a last sample.
- `device-registry`: Allow list/console live fields to show history-seeded values after reboot (exception to “only after new report / start-time read”), without persisting those fields.
- `automations`: History-seeded live values MUST NOT trigger status-path automation evaluation or action enqueue.

## Impact

- Host: `HistoryStore` (read last sample / `hasLast` after restore), `ZigbeeSpiProxy` live cache (`noteReportableValue` / main-state cache) with a silent seed path, boot ordering in `main.cpp` after `restoreSnapshot` + device map sync.
- Web consoles (`data/index.html`, `data/user.html`) consume existing live APIs—no separate “stale” UI marker in this change.
- Slave firmware unchanged.
- Host firmware build bump for this propose.
