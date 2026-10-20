# Spec Delta

## ADDED Requirements

### Requirement: Device binding may store a click action
A device binding on a console widget MAY optionally store one click action. When present, that action MUST use the same shape as an automation device/automation-target action (same fields the automation action editor persists). When omitted, the binding MUST keep today’s control behavior. Older console JSON without a click action MUST load with no click action.

#### Scenario: Click action persists
- **WHEN** the operator saves a binding with a click action that turns device B ON and the host reboots
- **THEN** that binding still has the same click action after reload

#### Scenario: Legacy binding without click action
- **WHEN** an older console JSON has a device binding with no click action field
- **THEN** view mode keeps the existing device control for that binding

### Requirement: Click action turns Place device into action button
When a device binding has a click action, Place and simple-user view mode MUST present that binding as an action button. Activating it MUST run the stored click action through the same execute path as automation actions. It MUST NOT perform the binding’s normal On/Off, covering, or other device-control gesture for that activation. Edit mode MUST still NOT send device commands or run click actions from widget controls.

#### Scenario: View mode runs click action
- **WHEN** a Place view-mode user activates a device binding that has a click action
- **THEN** the host runs that click action and does not toggle the bound device via the normal On/Off path

#### Scenario: Edit mode does not run click action
- **WHEN** the operator activates that same binding while the console editor dialog is open
- **THEN** the host does not run the click action and does not send device commands

#### Scenario: No click action keeps normal control
- **WHEN** a device binding has no click action and the user toggles On/Off in Place view mode
- **THEN** the host sends the normal On/Off command for that binding
