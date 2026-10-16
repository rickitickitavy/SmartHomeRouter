# Spec Delta

## MODIFIED Requirements

### Requirement: Device binding parameter selection

A device binding (not an automation binding) MUST allow selecting the displayed or commanded parameter using the same fields and dependent behavior as automation “device to trigger”: registered device, channel when multi-channel, param from the device’s parameter list, and when param is `custom`, cluster and attrId (hex). For a device with type `userData`, the param list MUST be the logical names from that device’s `userDataFields` (schema order) plus `custom`. Missing param on load MUST default to the current channel/status behavior used today for that device class; for `userData` with a non-empty schema and missing param, the host MAY default to the first field name. Automation bindings MUST NOT require these parameter fields.

#### Scenario: Custom attribute binding

- **WHEN** the editor saves a device binding with param `custom`, cluster, and attrId
- **THEN** after reload that binding still has those values and view mode shows that attribute’s value when available

#### Scenario: Default param omitted

- **WHEN** an older console binding has ieee and channel but no param
- **THEN** the host still loads it and view mode behaves as before this change

#### Scenario: User data field dropdown

- **WHEN** the editor binds a `userData` device whose schema includes `temp` and `label`
- **THEN** the param control lists those names and Save can store param `temp`
