# Spec Delta

## ADDED Requirements

### Requirement: Device dialog offers Rebind for Zigbee devices
When the operator opens the device parameter dialog to edit a registered Zigbee device, the dialog SHALL show a Rebind control. The control SHALL be hidden for MQTT-transport devices and for the add/create flow before the device is registered. Rebind SHALL NOT replace Save, Cancel, Export, or Delete.

#### Scenario: Edit Zigbee shows Rebind
- **WHEN** the operator opens Edit device for a registered Zigbee IEEE
- **THEN** the parameter dialog shows a Rebind control

#### Scenario: MQTT edit hides Rebind
- **WHEN** the operator opens Edit device for a registered MQTT device
- **THEN** the parameter dialog does not show Rebind

#### Scenario: Add Zigbee hides Rebind
- **WHEN** the operator opens the parameter dialog to add a found Zigbee device that is not yet saved
- **THEN** the parameter dialog does not show Rebind

### Requirement: Rebind control reports outcome
Activating Rebind SHALL call the host rebind API for that dialog’s IEEE and SHALL show success, pairing-fallback, or failure text in the dialog without closing the dialog or deleting the device. While the request is in flight, the Rebind control SHALL indicate busy and SHALL remain non-gray when enabled again after completion.

#### Scenario: Immediate success message
- **WHEN** rebind returns immediate success
- **THEN** the dialog shows a success message and the device remains open for editing

#### Scenario: Pairing fallback message
- **WHEN** rebind returns pairing fallback
- **THEN** the dialog shows that pairing is open for rebind and the registered device is kept
