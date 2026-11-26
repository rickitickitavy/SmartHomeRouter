## ADDED Requirements

### Requirement: Full backup history member does not DRAM-stage the archive
When Full backup includes historical data, the host SHALL stream history sample bytes into the ZIP member (`history/snapshot.bin` or equivalent) without copying that history payload into internal DRAM solely as ZIP staging, and without assembling the finished multi-member ZIP around it in RAM. Using PSRAM or an on-disk history snapshot file as the **member source** while streaming is allowed. Omitting history when Include historical data is off SHALL remain unchanged.

#### Scenario: History included without DRAM ZIP entry copy
- **WHEN** Full backup runs with Include historical data on and the history store has samples
- **THEN** those samples appear in the ZIP and the host does not keep a DRAM duplicate of the history payload for ZIP entry storage while also holding a finished archive buffer

#### Scenario: History omitted still omits member
- **WHEN** Full backup runs with Include historical data off
- **THEN** the ZIP omits history sample members as today
