# Spec Delta

## ADDED Requirements

### Requirement: Typed listen bindings
Each listening entry MUST identify a registered device, a channel when multi-channel, and either a known parameter key or a custom cluster plus attribute id. When a known parameter or custom attrId is set, the host MUST evaluate that automation on a status-change trigger only when that specific parameter/attribute event changes for the bound device (and channel). A change to a different attribute on the same device MUST NOT trigger that listener. Omitted typed fields on older JSON MUST keep prior cluster-only (or any-status) matching for compatibility.

#### Scenario: Specific attribute listen
- **WHEN** a listener is bound to window-covering Position and only Move Status changes
- **THEN** that automation is not evaluated for that change via the status path

#### Scenario: Custom attrId listen
- **WHEN** a listener uses CUSTOM cluster `0x0006` and attrId `0x0000` and that OnOff attribute reports a new value
- **THEN** the host evaluates that active automation’s condition

#### Scenario: Legacy listener without param
- **WHEN** an older automation JSON has a listener with ieee and optional cluster but no param/attrId
- **THEN** the host keeps the previous cluster-or-any matching behavior for that listener

### Requirement: Typed condition comparisons and custom VAL
A comparison MUST identify device, channel when multichannel, and either a known parameter or CUSTOM cluster plus attrId, plus compare type and compare value. Known-parameter comparisons MUST use the host’s interpreted status text for that parameter. CUSTOM comparisons MUST parse the live attribute as a numeric **VAL** and compare that number with the compare value (EQUAL, GREATER, LESS as numeric; string ops MAY be rejected or treated as invalid for CUSTOM).

#### Scenario: Custom VAL greater
- **WHEN** a CUSTOM comparison has GREATER and value `100` and the live attribute VAL is `150`
- **THEN** that comparison evaluates true

#### Scenario: Known Status equal
- **WHEN** a known On/Off Status comparison has EQUAL and value `ON` and the live status is ON
- **THEN** that comparison evaluates true

### Requirement: Typed device actions including custom command
A device-target action MUST identify device, channel when multichannel, and either a known parameter/command binding (with any required value such as light level or position) or CUSTOM with cluster, command id, and value. Known bindings MUST execute through the existing device-command path with the mapped payload. CUSTOM MUST send the specified cluster command and value to that device/channel. Automation-target actions are unchanged.

#### Scenario: Custom cluster command runs
- **WHEN** actions run a CUSTOM device action with cluster, command, and value
- **THEN** the host issues that command to the bound device channel

#### Scenario: Known On/Off action
- **WHEN** actions run a known On/Off Status action with ON
- **THEN** the host sends the same On command path as a console On payload for that device

## MODIFIED Requirements

### Requirement: Listening devices gate status-change evaluation
Each listening entry MUST identify a registered device and MAY specify a cluster, a known parameter, and/or an attribute id. The host MUST evaluate an active automation on a status-change trigger only when a listening device’s relevant status value changes for the bound scope (parameter/attr when set, else cluster when set, else any status for that device). A status update with the same value as the previous stored status MUST NOT trigger evaluation. If a listener has a cluster (and no tighter attr/param), only changes for that cluster MUST trigger that automation via the status path. TIMER expiry MUST still evaluate when `use` is true even if no listener matched.

#### Scenario: Same value ignored
- **WHEN** a listening device reports a status equal to its current status
- **THEN** no automation condition check runs for that report

#### Scenario: Cluster filter
- **WHEN** a listener specifies cluster X and the device changes status for cluster Y
- **THEN** that automation is not evaluated for that change via the status path

#### Scenario: Any status when unbound
- **WHEN** a listener has no cluster and no param/attrId and any status value for that device changes
- **THEN** the host evaluates that active automation’s condition

### Requirement: Condition trees use comparisons and containers
A condition MUST be a comparison or a container. Every condition MUST have a NOT flag. A container MUST have concatenation type AND, OR, or XOR and an ordered list of child conditions (comparisons or containers). A comparison MUST identify device, channel when the device is multichannel, optional cluster, optional known parameter or attrId, compare type (EQUAL, GREATER, LESS, CONTAINS, STARTS, ENDS), and compare value. When comparison uses a known parameter, the host MUST resolve that parameter’s interpreted status. When comparison is CUSTOM, the host MUST compare numeric VAL. When comparison omits typed parameter and cluster, the host MUST use the device’s default main status.

#### Scenario: Nested AND
- **WHEN** a container AND has two child comparisons both true
- **THEN** the container is true

#### Scenario: NOT inverts
- **WHEN** a comparison would be true and NOT is set
- **THEN** the condition is false

#### Scenario: Default main status
- **WHEN** a comparison omits cluster/param and the device’s default main status matches EQUAL to the compare value
- **THEN** the comparison is true
