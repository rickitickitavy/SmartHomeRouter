# Spec Delta

## ADDED Requirements

### Requirement: Registered devices store history epsilon
Each registered device SHALL persist a main-state history epsilon. Epsilon `< 0` means off (default/omitted `-1`); `>= 0` means on. When on, covering stores position and on/off stores per-channel state. The field SHALL round-trip host device store APIs; the slave SHALL NOT persist it.

#### Scenario: Default off
- **WHEN** the operator saves a new device without a history epsilon
- **THEN** the stored epsilon is `-1` and main-state history is off

#### Scenario: Enable survives reload
- **WHEN** the operator saves device history epsilon `0` and reloads the device store
- **THEN** that device still has epsilon `0` and main-state history is on

## MODIFIED Requirements

### Requirement: Devices store reportableData
Each registered device record (Zigbee or MQTT) SHALL store an optional `reportableData` array of at most 16 rows. Each row SHALL have `cluster` (uint16), `attribute` (uint16), `type` (`int` | `float` | `bool` | `string`), and `epsilon` of the same numeric kind as the value type except for `string`. For `int`/`float`/`bool`, epsilon `< 0` means history off and epsilon `>= 0` means history on with that deadband (`bool` uses the sign only: any change is stored when on). For `string`, epsilon is a flag only: `0` means history on, epsilon `< 0` means history off (no deadband). Default epsilon for new or omitted rows SHALL be `-1` (history off). Storage SHALL allocate memory only for the used row count. The field SHALL round-trip through `devices.json`, list JSON, single-device save/read, and export/import. Omitted on load SHALL mean an empty list. The slave SHALL NOT be required to persist this array (host-only, like `userDataFields`).

#### Scenario: Empty by default
- **WHEN** the operator creates a device without `reportableData`
- **THEN** the stored record has an empty reportable list unless create auto-fill seeds rows

#### Scenario: Persist two rows
- **WHEN** the operator saves a device with two reportable rows
- **THEN** after reload the record still has those two rows and host memory holds only two row slots

#### Scenario: Create auto-fill
- **WHEN** a new device is created with manufacturer Zbeacon (or a discovered measurement class) and the body omits `reportableData`
- **THEN** the host seeds temperature, humidity, and/or battery rows from manufacturer catalog merged with discovered class/cluster hints

#### Scenario: Reportable epsilon default off
- **WHEN** a reportable row is saved without epsilon
- **THEN** that row’s stored epsilon is `-1` and its history is off

#### Scenario: String history flag
- **WHEN** a string reportable row is saved with epsilon `0`
- **THEN** after reload that row still has epsilon `0` and string history is on
