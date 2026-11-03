# Spec Delta

## ADDED Requirements

### Requirement: Place optimistic actuators match simple-user
Place view-mode MUST apply the same optimistic actuator feedback rules as the simple-user page for switcher and icon toggles and for group On/Off members (immediate commanded state, up to 5 seconds for confirming live status, revert that control or member on timeout). Behavioral changes for this feedback MUST land on both pages.

#### Scenario: Place and user same timeout
- **WHEN** the same On/Off binding is toggled on Place and no confirming status arrives within 5 seconds
- **THEN** Place reverts that control the same way the simple-user page would for that binding

#### Scenario: Place group members independent
- **WHEN** Place group On/Off commands several On/Off devices
- **THEN** each member confirms or reverts on its own 5 second window as on the simple-user page
