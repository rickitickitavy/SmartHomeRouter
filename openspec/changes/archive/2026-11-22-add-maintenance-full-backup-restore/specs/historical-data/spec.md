## ADDED Requirements

### Requirement: Full backup may include history samples
When Maintenance Full backup runs with **Include historical data** enabled, the host SHALL include the persisted historical samples (the durable history store contents as kept for this board—LittleFS and/or SD according to current persist mode) in the Full backup ZIP. When that option is off, the ZIP SHALL omit history samples. History retention/slab settings remain part of the settings JSON slice as with Export settings.

#### Scenario: History option on packs samples
- **WHEN** Full backup runs with Include historical data on and the history store has samples
- **THEN** the ZIP contains those samples for restore

#### Scenario: History option off omits samples
- **WHEN** Full backup runs with Include historical data off
- **THEN** the ZIP does not contain history sample payloads

### Requirement: Full restore history replace or wipe
When Full restore imports historical data from a ZIP that contains history samples, the host SHALL replace the on-board history store with that payload. When Full restore does not import historical data, or the ZIP has no history samples, the host SHALL wipe the on-board history store so no pre-restore samples remain.

#### Scenario: Import replaces history
- **WHEN** Full restore imports historical data from a ZIP that includes samples
- **THEN** the history store matches the backup samples after restore

#### Scenario: No history import wipes store
- **WHEN** Full restore runs without importing historical data
- **THEN** the history store is empty after restore
