## MODIFIED Requirements

### Requirement: Binary sensor icon view style
For Binary Input / Binary Output / Binary Value (and BACnet binary) device bindings, and for On/Off device bindings and automation bindings, the binding MUST support view style `default` or `icon`. When view style is `icon`, the binding MUST store an icon id from the Material Design Icons set used by Home Assistant (`mdi:…`). Missing view style MUST behave as `default`. Binary sensor types MUST NOT offer gauge. On/Off and automation `default` MUST keep the existing switch control.

#### Scenario: Binary icon persists
- **WHEN** an editor saves a binaryInput binding with view style `icon` and icon `mdi:pump`
- **THEN** after reboot that binding still has those fields and view mode shows the pump icon

#### Scenario: On/Off icon persists
- **WHEN** an editor saves an On/Off binding with view style `icon` and icon `mdi:lightbulb`
- **THEN** after reboot that binding still has those fields

#### Scenario: Automation icon persists
- **WHEN** an editor saves an automation binding with view style `icon` and icon `mdi:timer`
- **THEN** after reboot that binding still has those fields

#### Scenario: Binary does not use gauge
- **WHEN** a binaryInput binding is edited
- **THEN** View style offers Default and Icon and does not offer Gauge

## ADDED Requirements

### Requirement: Icon view style is interactive for On/Off and automations
When view style is `icon` for an On/Off or automation binding, view mode MUST show the selected icon (not a switch) and MUST toggle the device ON/OFF or the automation `active` flag when the user activates the icon, using the same path as today’s switch. Binary sensor icons MAY stay display-only. ON/active MUST use yellow; OFF/inactive MUST use the muted theme color.

#### Scenario: User toggles On/Off via icon
- **WHEN** a simple user taps an On/Off binding shown as an icon while the device is OFF
- **THEN** the host sends ON for that device and channel the same way the switch would

#### Scenario: User toggles automation via icon
- **WHEN** a simple user taps an automation binding shown as an icon while the automation is active
- **THEN** that automation becomes inactive on the host

### Requirement: Editor icon preview is display-only for On/Off and automations
Editor preview of an On/Off or automation icon binding MUST NOT send device commands or change automation `active`.

#### Scenario: Editor icon is display-only
- **WHEN** an editor taps an On/Off or automation icon in the Consoles edit preview
- **THEN** no device command is sent and no automation `active` flag changes
