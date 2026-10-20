# Spec Delta

## ADDED Requirements

### Requirement: Device editor manages custom commands in memory until Save
The Devices parameter dialog MUST let the operator add, edit, and remove custom commands for the device being edited. Add MUST stay in-memory only; Cancel MUST discard unsaved edits. The first store write MUST be device Save. The UI MUST enforce the custom-command limits and types. Unsaved names MUST NOT appear in other pickers until after a successful Save and list refresh.

#### Scenario: Cancel discards new command
- **WHEN** the operator Adds a custom command in the device dialog and Cancels without Save
- **THEN** that command is not present after reload

#### Scenario: Save persists commands
- **WHEN** the operator Adds a custom command, fills a valid name, and Saves the device
- **THEN** list/export JSON includes that command and automation pickers can show it after refresh

#### Scenario: Add blocked at eight
- **WHEN** the device draft already has 8 custom commands
- **THEN** the dialog does not allow adding another until one is removed

### Requirement: Automation action UI shows custom commands and typed values
In the Automation action editor, when the target is a device that has saved custom commands, the command dropdown MUST include those commands. Selecting a custom command that requires a parameter MUST show the matching value control (`int` number, `bool` ON/OFF, `float` with two decimals, or `string`) and MUST require it filled before Save. Selecting a no-parameter custom command MUST hide/clear the custom-command value requirement.

#### Scenario: Parameter fields appear
- **WHEN** the operator selects custom command `level` that requires int
- **THEN** an int value field is shown and Save fails while it is empty

#### Scenario: No-parameter hides value
- **WHEN** the operator selects custom command `reboot` with no parameter
- **THEN** no custom-command parameter value field is required for that action

### Requirement: Place binding editor offers optional click action
In Place console edit mode, the device binding dialog MUST offer an optional click action configured with the same action editor controls as Automation actions (action type, target, command/parameter fields). Clearing the option MUST remove the stored click action. Preview/edit mode MUST NOT run the click action.

#### Scenario: Configure click action
- **WHEN** the operator enables click action on a binding, configures a device On action, and Saves the console
- **THEN** the binding stores that click action and Place view mode treats the control as an action button

#### Scenario: Clear click action
- **WHEN** the operator clears the click action on a binding and Saves
- **THEN** Place view mode restores normal device control for that binding
