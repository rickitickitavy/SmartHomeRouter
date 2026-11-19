# Spec Delta: web-console

## ADDED Requirements

### Requirement: Automation Time change trigger control
The automation edit dialog Trigger section MUST offer a **Time change** control (checkbox or equivalent) that persists with the automation. When on, the UI MUST present it as a minute-boundary schedule trigger (fires each local minute at :00). Operators MUST be able to turn it on without adding a listening device.

#### Scenario: Enable Time change without listeners
- **WHEN** an admin opens a new automation and turns Time change on with no listen rows
- **THEN** the dialog keeps Time change on and does not require a listen row for that control

### Requirement: Condition editors for time of day and day of week
When building a condition comparison on Current time, the Condition editor MUST let the operator choose **Time of day** or **Day of week**. Time of day MUST use an HH:MM value control and GREATER / EQUAL / LESS. Day of week MUST use Monday=1 … Sunday=7 (labels MAY show weekday names) and GREATER / EQUAL / LESS. EQUAL MUST be available for both (unlike the prior GREATER/LESS-only Current time UI).

#### Scenario: Time of day equal
- **WHEN** the operator picks Current time → Time of day, EQUAL, and `06:30`
- **THEN** the saved condition stores that shape for round-trip

#### Scenario: Day of week Sunday
- **WHEN** the operator picks Current time → Day of week, EQUAL, and Sunday
- **THEN** the saved compare value is `7`

## MODIFIED Requirements

### Requirement: Automation edit dialog import and export
Add or row edit MUST open an edit dialog with **General** (Active, then Name, then Group, Debug; no title), **Trigger** (countdown TIMER controls first, then **Time change**, then listening devices with typed parameter pickers), Condition (enable checkbox plus editor when enabled, with typed comparison pickers including Current time time-of-day / day-of-week), actions (device actions with typed pickers, automation targets, or **e-mail**), Export, and Import. Dialog Import MUST be enabled only when the form is empty (no listeners, no actions, condition disabled or empty, TIMER `use` false / defaults unset, Time change false). Dialog Export MUST download one JSON file named from the automation’s `name`.

#### Scenario: Group after name
- **WHEN** the operator opens the automation edit dialog
- **THEN** the Group field appears immediately below Name and accepts typed text or a choice from existing groups

#### Scenario: Empty-only dialog import
- **WHEN** the edit dialog already has a listener, action, TIMER `use` true, or Time change true
- **THEN** Import is disabled

#### Scenario: No Title field
- **WHEN** the operator opens the automation edit dialog
- **THEN** there is no Title input

#### Scenario: Email among action targets
- **WHEN** the operator opens the Actions editor target control
- **THEN** the options include device, automation, and e-mail
