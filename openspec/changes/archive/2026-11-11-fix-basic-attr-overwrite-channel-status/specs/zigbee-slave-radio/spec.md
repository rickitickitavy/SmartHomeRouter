## ADDED Requirements

### Requirement: Basic cluster reports stay off the channel-status path
When the slave receives a ZCL Basic (`0x0000`) attribute report or read response for a usable endpoint, it MUST log and pulse inbound traffic as today. ManufacturerName (`0x0004`) and ModelIdentifier (`0x0005`) MUST still update bound-device identity. The slave MUST NOT forward Basic attributes (including ApplicationVersion `0x0001`, ZCLVersion, stack/HW version, or other Basic attrs) onto the host channel-status / light-state path as `ON`/`OFF` or as shaped `cl=/attr=/val=` status text.

#### Scenario: ApplicationVersion does not become status traffic
- **WHEN** a registered On/Off device reports Basic ApplicationVersion `attr=0x0001` with value `0x53`
- **THEN** the slave does not send that value to the host as a channel status message (no `cl=0x0000,attr=0x0001,…` status body and no `ON`/`OFF` substitution from that attr)

#### Scenario: ManufacturerName still updates identity
- **WHEN** a device answers Basic ManufacturerName during identity read
- **THEN** the slave still stores that manufacturer string on the bound device as before
