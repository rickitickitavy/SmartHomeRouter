# Spec Delta

## ADDED Requirements

### Requirement: User-data MQTT uses brace-list payload

MQTT traffic for `userData` devices MUST use a `{…}` brace list matching `userDataFields` order, without a `cl=`/`attr=` wrapper. Null slots are `NULL` or empty (`,,`). Bool accepts `true`/`false` and `on`/`off`. Internally the list maps to cluster `0xFC00` attr `0x0001`. Outbound state publish MUST use the same brace-list form.

#### Scenario: MQTT state brace list

- **WHEN** an MQTT `userData` device with three schema fields receives state `{1000,,ON}`
- **THEN** the host maps slot 0 and slot 2 to the first and third fields and treats the middle field as null/empty

#### Scenario: Publish brace list only

- **WHEN** the host publishes MQTT state for a `userData` device
- **THEN** the body is a `{…}` list without a leading `cl=0xFC00` prefix
