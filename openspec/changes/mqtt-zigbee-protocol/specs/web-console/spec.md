# Spec Delta

## ADDED Requirements

### Requirement: MQTT device dialog includes Zigbee protocol
The device parameter dialog SHALL show a **Zigbee protocol** checkbox immediately after **DEVICE SOURCE** when the dialog transport is `mqtt`, for every MQTT device class including User data. The checkbox SHALL be hidden for `zigbee` transport. It SHALL load from the stored `zigbeeProtocol` flag and SHALL be unchecked when the flag is omitted or false. Save SHALL persist the checkbox with the `mqtt` device. The control SHALL be a checkbox (square-only toggle), not a text field or on/off select. Unlike DEVICE SOURCE, Zigbee protocol SHALL remain editable on later edits of an existing `mqtt` device (operator may turn it on or off and Save).

#### Scenario: Visible on MQTT create
- **WHEN** the operator opens the MQTT DEVICE create dialog
- **THEN** Zigbee protocol appears immediately after DEVICE SOURCE and is unchecked

#### Scenario: Visible for User data
- **WHEN** the operator opens parameters for an `mqtt` device with class User data
- **THEN** Zigbee protocol appears immediately after DEVICE SOURCE

#### Scenario: Hidden on Zigbee
- **WHEN** the operator opens parameters for a `zigbee` device
- **THEN** the Zigbee protocol checkbox is not shown

#### Scenario: Save persists
- **WHEN** the operator checks Zigbee protocol on an `mqtt` device and Saves
- **THEN** the stored record has `zigbeeProtocol` true

#### Scenario: Editable on later edit
- **WHEN** the operator opens parameters for a stored `mqtt` device with `zigbeeProtocol` false, checks Zigbee protocol, and Saves
- **THEN** the stored record has `zigbeeProtocol` true

#### Scenario: Can turn off on later edit
- **WHEN** the operator opens parameters for a stored `mqtt` device with `zigbeeProtocol` true, unchecks Zigbee protocol, and Saves
- **THEN** the stored record has `zigbeeProtocol` false

### Requirement: Reportable data editor
The device parameter dialog SHALL let the operator edit `reportableData` rows (cluster, attribute, type) for Zigbee and MQTT devices, with add/remove controls and a maximum of 16 rows. Save SHALL persist the list. Create MAY show rows prefilled from auto-fill; Cancel of an unsaved create SHALL NOT leave a stored device.

#### Scenario: Edit and save
- **WHEN** the operator adds a temperature row and Saves
- **THEN** the stored device includes that reportable row after reload

### Requirement: Widget Reported data picker
When a console widget binds a device that has one or more reportable rows, the binding UI SHALL offer a **Reported data** control in addition to Channel. The binding SHALL record the chosen cluster/attribute. Display SHALL use the runtime last value for that endpoint plus cluster/attribute.

#### Scenario: Pick humidity
- **WHEN** a device has temperature and humidity reportable rows and the operator selects humidity on the widget with channel 1
- **THEN** the widget shows the last humidity value for endpoint 1 without changing Channel semantics
