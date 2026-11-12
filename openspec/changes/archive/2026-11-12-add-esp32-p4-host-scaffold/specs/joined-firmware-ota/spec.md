## MODIFIED Requirements

### Requirement: Independent host and slave firmware version macros
The firmware SHALL define distinct host and slave version strings. The host SHALL use a SoC-prefixed identity `S3-MAJOR.MINOR.BUILD` or `P4-MAJOR.MINOR.BUILD` via `FIRMWARE_VERSION_HOST` (same build number; prefix only). The slave SHALL use unprefixed `FIRMWARE_VERSION_SLAVE` as `MAJOR.MINOR.BUILD`. The host application SHALL identify itself with the prefixed host string. The slave application SHALL identify itself with `FIRMWARE_VERSION_SLAVE`. Status and OTA expectations SHALL use the matching chip’s identity string, not a single shared string for both chips.

#### Scenario: Prefixed host vs unprefixed slave
- **WHEN** the S3 host core is `4.2.22` and `FIRMWARE_VERSION_SLAVE` is `4.2.04`
- **THEN** Status shows master `S3-4.2.22` and slave `4.2.04`

#### Scenario: P4 host prefix
- **WHEN** the P4 host image runs with the same core `4.2.22`
- **THEN** Status shows master `P4-4.2.22`
