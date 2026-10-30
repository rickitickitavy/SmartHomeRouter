# Spec Delta

## MODIFIED Requirements

### Requirement: Edit mode does not command devices

While a console is open in the editor dialog, the host MUST NOT send On/Off, covering, or other device commands from widget or row controls, and MUST NOT change automation `active` from those controls. View mode on Place and the simple-user page MUST send device commands using the same base device-command path as Manual command / Devices row actions (honoring **use queue for base device command**), and MUST set automation `active` when the user toggles an automation row. Automation Run / timer control modes MUST use the automation action paths that enqueue on the action/command FIFO.

#### Scenario: Editor flips a switch

- **WHEN** the editor toggles an On/Off control inside the unsaved edit dialog
- **THEN** the device is not commanded and no automation `active` flag is changed

#### Scenario: User flips automation Off

- **WHEN** a simple user toggles an automation row Off on an assigned console
- **THEN** that automation becomes inactive on the host

#### Scenario: User flips a device switch

- **WHEN** a simple user toggles an On/Off device control on an assigned console
- **THEN** the host sends the On/Off command for that device and channel via the base device-command path (immediate or queued per the Hardware flag)

## ADDED Requirements

### Requirement: View-mode device commands follow hardware queue flag

In view mode, widget and group On/Off / covering / other live device controls on Place and simple-user consoles MUST use the base device-command path for all device kinds. When **use queue for base device command** is true, those commands MUST enter the action/command FIFO; when false, they MUST remain immediate. Configure-dialog writes remain out of scope for consoles.

#### Scenario: Flag on queues group Off

- **WHEN** the flag is true and the user turns a widget group switch Off for three On/Off bindings
- **THEN** the three OFF commands are enqueued (or otherwise paced via the FIFO) rather than all applied inline on the click handler with no queue

#### Scenario: Flag off keeps covering immediate

- **WHEN** the flag is false and the user presses OPEN on a covering binding
- **THEN** OPEN is applied on the immediate device-command path
