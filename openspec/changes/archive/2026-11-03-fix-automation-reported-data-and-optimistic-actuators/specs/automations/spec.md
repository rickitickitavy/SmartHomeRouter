# Spec Delta

## ADDED Requirements

### Requirement: Reported data param keys on listen and condition
A listen entry or condition comparison MAY use a Reported data param key of the form `reportable:<cluster>/<attribute>` (same ids the console offers for that device’s `reportableData` rows). The host MUST accept such keys on save when the rest of the automation is valid. The host MUST NOT reject the save solely because the param is Reported data rather than a catalog key such as `sensorValue` or `status`.

#### Scenario: Condition with reported data saves
- **WHEN** the operator saves an automation with TIMER `use` false, a listening device, condition enable true, and a comparison whose param is a valid Reported data key for a registered device with a non-empty compare value
- **THEN** the host accepts the save

#### Scenario: Listener with reported data saves
- **WHEN** the operator saves an automation whose listening entry param is a valid Reported data key for that device
- **THEN** the host accepts the save

#### Scenario: Sensor value still saves
- **WHEN** the operator saves the same automation shape using param `sensorValue` instead of Reported data
- **THEN** the host still accepts the save

### Requirement: Reported data triggers status-path evaluation
When a listening entry uses a Reported data param key, the host MUST evaluate that automation on the status path only when a live report for that device (and channel when set) matches the bound cluster/attribute scope for that key, subject to the listener’s event type (`change` vs `any`) the same way as other typed listeners.

#### Scenario: Matching reported attribute triggers
- **WHEN** an active automation listens on Reported data for cluster/attribute pair X/Y and that device reports a new converted value for X/Y
- **THEN** the host evaluates that automation’s condition

#### Scenario: Different reported attribute does not trigger
- **WHEN** an active automation listens on Reported data for cluster/attribute pair X/Y and the device reports only a different reportable pair
- **THEN** that automation is not evaluated for that report via that listener

### Requirement: Reported data is comparable in conditions
When a condition comparison uses a Reported data param key, the host MUST resolve the live comparable text from that device’s current reportable value for the bound cluster/attribute (and channel when set) and MUST apply the comparison op against the stored compare value the same way as for other typed numeric or text status values (EQUAL / GREATER / LESS / string ops as already supported).

#### Scenario: Reported numeric greater
- **WHEN** a Reported data comparison uses GREATER with value `20` and the live converted reportable value is `21.5`
- **THEN** that comparison evaluates true

#### Scenario: Reported equal text or number
- **WHEN** a Reported data comparison uses EQUAL with a compare value that matches the live converted reportable text
- **THEN** that comparison evaluates true

#### Scenario: Unresolved reported value is false
- **WHEN** a Reported data comparison has no live reportable value yet for that cluster/attribute
- **THEN** that comparison evaluates false (unless NOT inverts it)
