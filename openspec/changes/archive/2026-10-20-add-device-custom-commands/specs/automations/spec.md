# Spec Delta

## ADDED Requirements

### Requirement: Device actions offer registered custom commands
When an automation device-target action selects a registered device, the command dropdown MUST include that device’s saved custom commands in addition to existing known bindings and ZCL CUSTOM. Selecting a custom command MUST identify that command by name on the action. Existing ZCL CUSTOM (cluster/command id/value) behavior MUST remain unchanged.

#### Scenario: Custom command listed
- **WHEN** device A has custom command `reboot` and the operator opens a device action for A
- **THEN** the command dropdown includes `reboot`

#### Scenario: Other device commands not listed
- **WHEN** device A has custom command `reboot` and the operator opens a device action for device B that has no custom commands
- **THEN** the command dropdown for B does not include `reboot`

### Requirement: Custom command action requires parameter value when needed
When the selected custom command requires a parameter, the automation action editor MUST show a value control typed for that parameter (`int`, `bool` ON/OFF, `float` with two decimals, or `string`). The value MUST be non-empty and valid for the type before the automation can be saved. When the command has no parameter, the editor MUST NOT require a value field for that custom command.

#### Scenario: Missing bool value rejected
- **WHEN** the operator selects a custom command that requires bool and leaves the value empty
- **THEN** the host rejects the automation save

#### Scenario: Filled int value accepted
- **WHEN** the operator selects a custom command that requires int, enters `25`, and saves
- **THEN** the automation stores that action with the custom command name and value `25`

### Requirement: Running a custom command action executes device format
When actions run a device-target action bound to a registered custom command, the host MUST execute that command on the bound device and channel using the MQTT or Zigbee custom-command payload format for that device’s transport, with the stored parameter value when required.

#### Scenario: MQTT custom action fires
- **WHEN** actions run a custom command `level=40` on an MQTT device
- **THEN** the host publishes `level=40` on that device’s command topic

#### Scenario: Zigbee custom action fires
- **WHEN** actions run a no-parameter custom command `identify` on a Zigbee device
- **THEN** the host issues body `cmd=identify` on that device’s command path
