# Spec Delta

## ADDED Requirements

### Requirement: Registered devices store manufacturer enum
Each registered device record MUST store a manufacturer enum (`unknown`, `tuya`, `aqara`, `philipsHue`, `ikea`, `sonoff`, `ledvance`, `innr`, `gledopto`, `sengled`, `thirdReality`, `heiman`, `legrand`, `schneider`, `namron`, `develco`) as a persisted settings field through `devices.json`, list JSON after reboot, single-device save/read, and export/import. The slave MUST NOT be required to store the enum.

#### Scenario: Tuya join maps and persists
- **WHEN** the operator saves a found Zigbee device whose join manufacturer is a Tuya string
- **THEN** the registered record stores manufacturer `tuya` and that value is present after reboot and in export JSON

#### Scenario: New save omits manufacturer
- **WHEN** a new record is saved without a manufacturer field
- **THEN** the stored manufacturer is `unknown`

#### Scenario: Legacy record without field
- **WHEN** the host loads a device record with no manufacturer key
- **THEN** the in-memory manufacturer is `unknown`

#### Scenario: Parameter save does not clobber known manufacturer
- **WHEN** a Zigbee device already stores manufacturer `aqara` and the operator saves parameters with a different manufacturer in the body
- **THEN** the stored manufacturer remains `aqara`

#### Scenario: MQTT default
- **WHEN** an MQTT device is created without a manufacturer
- **THEN** the stored manufacturer is `unknown`

### Requirement: Device list JSON includes manufacturer
Host registered-device list APIs and the persisted device store MUST include the stored manufacturer enum JSON id for each registered device so the console can decide Configure eligibility and display the brand.

#### Scenario: List shows manufacturer
- **WHEN** the console loads the devices list for a registered Tuya device
- **THEN** that device’s JSON includes `"manufacturer":"tuya"` (or the project’s chosen JSON id for that enum)
