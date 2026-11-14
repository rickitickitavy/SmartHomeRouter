# Spec Delta

## ADDED Requirements

### Requirement: Slave status carries CPU busy percent
Slave-to-host status replies SHALL include the slave’s current CPU busy percent (0–100) when a sample is available. The host SHALL cache that value for gateway Status. Missing or unparsable CPU in a status reply SHALL leave the previous cache unchanged or mark slave CPU unavailable; it SHALL NOT invent 0%. SPI OTA begin/chunk/end framing SHALL remain unchanged.

#### Scenario: Status reply updates cache
- **WHEN** the slave answers a status request with busy percent 37
- **THEN** the host caches slave CPU 37 for Status

#### Scenario: No sample stays unavailable
- **WHEN** the host has never received a slave CPU field (or the slave is unlinked with no valid cache policy)
- **THEN** gateway Status treats slave CPU as unavailable and MUST NOT display a fabricated 0%
