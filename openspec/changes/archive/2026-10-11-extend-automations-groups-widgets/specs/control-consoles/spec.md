# Spec Delta: control-consoles

## MODIFIED Requirements

### Requirement: Consoles persist with widgets and devices
The host MUST persist control consoles. Each console MUST have a unique id, a name, an active flag, and an ordered list of widgets. Each widget MUST have a title, a show-group-on-off flag, and an ordered list of bindings. A binding MUST target either a registered device (optional channel, title, optional unit) or an automation by `name` (optional row title). Empty consoles MUST be allowed. After reboot the same consoles MUST load. The host MUST allow at most 32 consoles. Device-only exports from older firmware (bindings without automation targets) MUST still load.

#### Scenario: Save and reload with automation binding
- **WHEN** an editor saves console `Kitchen` with one widget that binds automation `yard-light`
- **THEN** after reboot `Kitchen` still has that widget and automation binding

#### Scenario: Thirty-third console rejected
- **WHEN** the host already has 32 consoles and an editor saves a new one
- **THEN** the host does not persist the thirty-third console and returns an error

### Requirement: Device bindings match device class
An editor MUST add a device from registered devices, pick a channel when the device has more than one channel, and set a row title. For a measurement sensor the editor MUST pick a unit from a list for that class (temperature, pressure, humidity, illuminance, flow, speed, direction, and similar). On/Off rows MUST show a switch. Window covering rows MUST show up, down, stop, and current position. Sensor rows MUST show the live value in the chosen unit. An editor MUST also be able to add an automation binding from stored automations; that row MUST show an On/Off control reflecting the automation’s `active` flag (On = active).

#### Scenario: Multi-channel On/Off
- **WHEN** the editor adds a two-channel On/Off device and selects channel 2
- **THEN** the row controls only channel 2

#### Scenario: Automation row shows active as On/Off
- **WHEN** a widget binds automation `timer-light-on` that is active
- **THEN** the row switch shows On

#### Scenario: Temperature unit
- **WHEN** the editor adds a temperature sensor
- **THEN** the unit list includes Celsius, Fahrenheit, and Kelvin

### Requirement: Edit mode does not command devices
While a console is open in the editor dialog, the host MUST NOT send On/Off, covering, or other device commands from widget or row controls, and MUST NOT change automation `active` from those controls. View mode on the user page MUST send device commands using the same device actions as today, and MUST set automation `active` when the user toggles an automation row.

#### Scenario: Editor flips a switch
- **WHEN** the editor toggles an On/Off control inside the unsaved edit dialog
- **THEN** the device is not commanded and no automation `active` flag is changed

#### Scenario: User flips automation Off
- **WHEN** a simple user toggles an automation row Off on an assigned console
- **THEN** that automation becomes inactive on the host

#### Scenario: User flips a device switch
- **WHEN** a simple user toggles an On/Off device control on an assigned console
- **THEN** the host sends the On/Off command for that device and channel

## ADDED Requirements

### Requirement: Widget group On/Off ignores automation rows
The widget-level group On/Off control MUST continue to aggregate and command only bound On/Off **devices**. Automation bindings MUST NOT affect the group switch state and MUST NOT be toggled by the group switch.

#### Scenario: Group on from devices only
- **WHEN** a widget has one On/Off device that is Off and one automation that is active
- **THEN** the group switch shows Off

#### Scenario: Group toggle skips automations
- **WHEN** the user turns the group switch On
- **THEN** bound On/Off devices are commanded On and automation `active` flags are unchanged
