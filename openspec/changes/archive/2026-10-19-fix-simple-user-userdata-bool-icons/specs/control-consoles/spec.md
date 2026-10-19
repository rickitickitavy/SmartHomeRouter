# Spec Delta

## MODIFIED Requirements

### Requirement: Binary sensor icon view style
For Binary Input / Binary Output / Binary Value (and BACnet binary) device bindings, On/Off device bindings, automation bindings, and `userData` bindings whose selected schema field type is `bool`, the binding MUST support view style `default` or `icon`. When view style is `icon`, the binding MUST store an icon id from the Material Design Icons set used by Home Assistant (`mdi:…`). Missing view style MUST behave as `default`. Binary sensor types and `userData` bool fields MUST NOT offer gauge. On/Off and automation `default` MUST keep the existing switch control. `userData` bool `default` MUST show the boolean state as text (ON/OFF), not a switch.

#### Scenario: Binary icon persists
- **WHEN** an editor saves a binaryInput binding with view style `icon` and icon `mdi:pump`
- **THEN** after reboot that binding still has those fields and view mode shows the pump icon

#### Scenario: On/Off icon persists
- **WHEN** an editor saves an On/Off binding with view style `icon` and icon `mdi:lightbulb`
- **THEN** after reboot that binding still has those fields

#### Scenario: Automation icon persists
- **WHEN** an editor saves an automation binding with view style `icon` and icon `mdi:timer`
- **THEN** after reboot that binding still has those fields

#### Scenario: UserData bool icon persists
- **WHEN** an editor saves a `userData` bool field binding with view style `icon` and icon `mdi:water`
- **THEN** after reboot that binding still has those fields and simple-user view mode shows the water icon

#### Scenario: Binary does not use gauge
- **WHEN** a binaryInput binding is edited
- **THEN** View style offers Default and Icon and does not offer Gauge
