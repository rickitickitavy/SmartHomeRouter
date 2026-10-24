# device-custom-commands Specification

## Purpose

Defines per-device named custom commands with optional typed parameters and the MQTT versus Zigbee execute payload formats operators and automations rely on.

## Requirements

### Requirement: Device custom command limits and shape
Each registered device MAY store up to 8 custom commands. Each command MUST have a non-empty name of at most 11 characters (plus NUL terminator in storage). Each command MAY require exactly one parameter, or none. When a parameter is required, its type MUST be one of `int`, `bool`, `float`, or `string`. A `string` parameter value MUST be at most 15 characters (plus NUL). Command names on one device MUST be unique (case-sensitive).

#### Scenario: Max eight commands
- **WHEN** the operator tries to save a ninth custom command on a device
- **THEN** the host rejects that save and keeps the previous command list

#### Scenario: Name length enforced
- **WHEN** the operator saves a custom command whose name has more than 11 characters
- **THEN** the host rejects that save

#### Scenario: Duplicate names rejected
- **WHEN** the operator saves two custom commands with the same name on one device
- **THEN** the host rejects that save

### Requirement: Parameter type encoding on execute
When a custom command with a required parameter executes, the value MUST be encoded as: `int` decimal digits; `bool` the literal strings `ON` or `OFF`; `float` with exactly two digits after the decimal point; `string` the raw characters up to 15. Commands without a parameter MUST NOT append a value.

#### Scenario: Bool ON
- **WHEN** a custom command with bool parameter executes with value ON
- **THEN** the outbound payload uses the substring `ON` as the value

#### Scenario: Float two digits
- **WHEN** a custom command with float parameter executes with value 12.3
- **THEN** the outbound value text is `12.30`

#### Scenario: No parameter omits value
- **WHEN** a custom command without a parameter executes
- **THEN** the outbound payload has no parameter value segment

### Requirement: MQTT transport execute format
For a device with transport `mqtt`, executing a custom command MUST publish to that device’s mapped command topic. The payload MUST be `${command}` when no parameter is required, or `${command}=${value}` when a parameter is required, using the encoded value rules for that type.

#### Scenario: MQTT command only
- **WHEN** MQTT device `lamp` runs custom command `reboot` with no parameter
- **THEN** the host publishes payload `reboot` on that device’s command topic

#### Scenario: MQTT command with value
- **WHEN** MQTT device runs custom command `level` with int parameter `40`
- **THEN** the host publishes payload `level=40` on that device’s command topic

### Requirement: Zigbee transport execute format
For a device with transport `zigbee`, executing a custom command MUST send a command body through the existing device-command path. The body MUST be `cmd=${command}` when no parameter is required, or `cmd=${command},val=${value}` when a parameter is required. The host MUST NOT invent a `val=` key when no parameter is required.

#### Scenario: Zigbee command only
- **WHEN** a Zigbee device runs custom command `identify` with no parameter
- **THEN** the host issues body `cmd=identify`

#### Scenario: Zigbee command with value
- **WHEN** a Zigbee device runs custom command `scene` with string parameter `hall`
- **THEN** the host issues body `cmd=scene,val=hall`
