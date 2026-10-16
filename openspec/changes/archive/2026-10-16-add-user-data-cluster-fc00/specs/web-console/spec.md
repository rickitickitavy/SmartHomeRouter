# Spec Delta

## ADDED Requirements

### Requirement: User-data parameter dialog edits field schema

For type `userData`, the parameter dialog MUST show a User data fields table (columns: nullable checkbox, name, type) with per-row Remove, Up, and Down. Add is draft-only until Save; Cancel discards. Types are `int`, `float`, `bool`, `string`. Hide the table for other types.

#### Scenario: Reorder then save

- **WHEN** the operator edits a `userData` device, moves the second field above the first, and Saves
- **THEN** after reload the schema list order matches the reordered table

#### Scenario: Cancel discards draft rows

- **WHEN** the operator Adds a field row in the dialog and Cancels without Save
- **THEN** after reopen the stored schema does not include that unsaved row

#### Scenario: Hidden for On/Off

- **WHEN** the operator opens parameters for an `onOff` device
- **THEN** the User data fields table is not shown

## MODIFIED Requirements

### Requirement: Device parameters show readonly type

For a `zigbee` device, the device parameter dialog SHALL show the cluster device type as a readonly field. Add from Search SHALL show the found type. Edit of a registered `zigbee` device SHALL show the stored type. Save of a `zigbee` device SHALL persist other editable fields and SHALL NOT take a new type from an editable control. For an `mqtt` device, the cluster device type SHALL be an editable control among Unknown, On/Off, IAS Zone, Window covering, and User data, and Save SHALL persist the chosen type.

#### Scenario: Add shows found type

- **WHEN** the operator Adds a found `iasZone` device
- **THEN** the parameter dialog shows IAS Zone and the operator cannot edit that field

#### Scenario: Edit keeps stored type

- **WHEN** the operator opens parameters for a registered `zigbee` `onOff` device and saves a new name
- **THEN** the type remains `onOff` and the dialog still shows On/Off

#### Scenario: WiFi type editable

- **WHEN** the operator opens parameters for a registered `mqtt` device
- **THEN** the cluster type control is editable and Save can change the stored type

#### Scenario: MQTT User data selectable

- **WHEN** the operator opens parameters for a registered `mqtt` device
- **THEN** the type control includes User data and Save can store type `userData`

### Requirement: Known parameter lists by device type

The console MUST offer known parameters based on the device’s classified type (and related clusters when present), including at least: Window covering — Status (STOPPED, UP, DOWN) and Position; On/Off — Status (ON, OFF); Level/dimmer — Status (ON, OFF) and Light level; measurement sensors — Sensor value (and Battery level when the device exposes battery); IAS zone — zone Status (LEAK/DRY style strings the host already publishes); User data — each logical name from that device’s persisted `userDataFields` in schema order. Devices with no matching catalog entry MUST still offer CUSTOM. For `userData`, CUSTOM remains available in addition to the named fields.

#### Scenario: Window covering known list

- **WHEN** the operator selects a window-covering device on Trigger or Condition
- **THEN** the parameter list includes Status and Position plus CUSTOM

#### Scenario: User data named fields

- **WHEN** the operator selects a `userData` device whose schema has fields `temp` and `label` on Trigger, Condition, Actions, or a console binding param picker
- **THEN** the parameter list includes `temp` and `label` (in that order) plus CUSTOM
