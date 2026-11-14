# Spec Delta

## MODIFIED Requirements

### Requirement: Device list JSON includes runtime status and battery
Host registered-device list APIs SHALL include last known per-endpoint status and battery percent when the host has received those values. Status entries SHALL include the source endpoint (`ep`) and the last type-specific payload for that endpoint. Battery SHALL be an integer percent derived from Power Configuration Battery Percentage Remaining when a valid report exists. These fields are runtime telemetry: they SHALL NOT be written into the persisted device list, `devices.json`, or EEPROM device slots. A list replace, export, or restore SHALL NOT require or store these fields. After reboot, values MAY appear from a history-seeded live cache when a last history sample exists for that series; otherwise they SHALL appear only after a new report or a start-time read.

#### Scenario: List after on/off report
- **WHEN** the operator requests the registered device list and endpoint 2 of that IEEE last reported `ON`
- **THEN** that device’s JSON includes a status entry for `ep` `2` with payload `ON`

#### Scenario: List after battery report
- **WHEN** the host has a valid battery percentage remaining that decodes to 67 percent
- **THEN** that device’s list JSON includes battery `67`

#### Scenario: Persist does not keep telemetry
- **WHEN** the host saves or exports the device list after it has cached battery and status
- **THEN** the persisted document has no battery or live status fields

#### Scenario: History seed fills list after reboot
- **WHEN** the host reboots, restores history with a last sample for a reportable attribute, and seeds the live cache
- **THEN** registered-device list JSON includes that reportable live value before a new radio/MQTT report for it
