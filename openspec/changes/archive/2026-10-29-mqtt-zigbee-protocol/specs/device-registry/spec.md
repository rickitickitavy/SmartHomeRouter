# Spec Delta

## ADDED Requirements

### Requirement: MQTT devices store zigbeeProtocol
Each registered `mqtt` device record SHALL store a boolean `zigbeeProtocol`. The field SHALL appear in `devices.json`, device list JSON after reboot, single-device save/read, and export/import. Default SHALL be false when omitted on create or load. Later saves of an existing `mqtt` record SHALL accept changes to `zigbeeProtocol` (it is not create-only). Non-`mqtt` records SHALL NOT store or require this field. The slave SHALL NOT be required to persist or interpret this flag.

#### Scenario: New MQTT omits flag
- **WHEN** the operator creates an `mqtt` device without `zigbeeProtocol`
- **THEN** the stored record has `zigbeeProtocol` false

#### Scenario: Persist on
- **WHEN** the operator saves an `mqtt` device with `zigbeeProtocol` true
- **THEN** after reload and export the record still has `zigbeeProtocol` true

#### Scenario: Later edit changes flag
- **WHEN** a stored `mqtt` device has `zigbeeProtocol` false and a later save sets `zigbeeProtocol` true
- **THEN** the stored record has `zigbeeProtocol` true after reload

#### Scenario: Zigbee transport excluded
- **WHEN** the operator saves or loads a `zigbee` transport device
- **THEN** the record does not require `zigbeeProtocol`

### Requirement: Devices store reportableData
Each registered device record (Zigbee or MQTT) SHALL store an optional `reportableData` array of at most 16 rows. Each row SHALL have `cluster` (uint16), `attribute` (uint16), and `type` (`int` | `float` | `bool` | `string`). Storage SHALL allocate memory only for the used row count. The field SHALL round-trip through `devices.json`, list JSON, single-device save/read, and export/import. Omitted on load SHALL mean an empty list. The slave SHALL NOT be required to persist this array (host-only, like `userDataFields`).

#### Scenario: Empty by default
- **WHEN** the operator creates a device without `reportableData`
- **THEN** the stored record has an empty reportable list unless create auto-fill seeds rows

#### Scenario: Persist two rows
- **WHEN** the operator saves a device with two reportable rows
- **THEN** after reload the record still has those two rows and host memory holds only two row slots

#### Scenario: Create auto-fill
- **WHEN** a new device is created with manufacturer Zbeacon (or a discovered measurement class) and the body omits `reportableData`
- **THEN** the host seeds temperature, humidity, and/or battery rows from manufacturer catalog merged with discovered class/cluster hints

### Requirement: Manufacturer includes Zbeacon
The device manufacturer catalog SHALL include `zbeacon` (label Zbeacon). Join identity strings containing `zbeacon` (case-insensitive) SHALL map to that manufacturer.

#### Scenario: Join maps Zbeacon
- **WHEN** a found device reports manufacturer name `Zbeacon`
- **THEN** the stored manufacturer id is `zbeacon`
