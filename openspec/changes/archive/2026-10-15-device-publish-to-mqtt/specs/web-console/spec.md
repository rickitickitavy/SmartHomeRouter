# Spec Delta

## ADDED Requirements

### Requirement: Device edit dialog groups MQTT fields
The device parameters (edit) dialog SHALL place MQTT topic fields inside a labeled **MQTT** container/group. For a device whose transport is not `mqtt`, the dialog SHALL show a **Publish to MQTT** checkbox before those topic fields. When that checkbox is unchecked, the MQTT topic fields SHALL be hidden. When checked, the topic fields SHALL be visible for editing. For `mqtt` transport devices, the checkbox SHALL NOT appear; topic fields remain available as today inside the MQTT group.

#### Scenario: Zigbee unchecked hides topics
- **WHEN** the operator edits a `zigbee` device and Publish to MQTT is unchecked
- **THEN** state, command, and availability topic fields are not shown

#### Scenario: Zigbee checked shows topics
- **WHEN** the operator edits a `zigbee` device and checks Publish to MQTT
- **THEN** the MQTT topic fields are shown inside the MQTT group below the checkbox

#### Scenario: MQTT device has no checkbox
- **WHEN** the operator edits an `mqtt` transport device
- **THEN** Publish to MQTT is not shown and topic fields remain editable in the MQTT group

#### Scenario: Save persists checkbox
- **WHEN** the operator unchecks Publish to MQTT on a zigbee device and Saves
- **THEN** after reload the checkbox is unchecked and topic fields stay hidden until checked again
