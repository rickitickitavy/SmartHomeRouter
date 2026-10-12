# Spec Delta

## ADDED Requirements

### Requirement: Optional condition enable flag
Each automation MUST persist a boolean that enables the condition tree (default true for existing automations that omit the field). When that flag is false, the host MUST NOT require a condition tree on save and MUST treat the automation’s condition as true whenever a trigger would evaluate it. When the flag is true, existing validation for non-empty conditions still applies together with TIMER rules.

#### Scenario: Disabled condition on timer finish
- **WHEN** an active automation has condition enable false, TIMER `use` true, and the countdown finishes
- **THEN** the host runs its actions without requiring a condition tree

#### Scenario: Omitted flag defaults true
- **WHEN** an older automation JSON without the condition-enable field loads
- **THEN** the host treats condition enable as true

### Requirement: Debug logs trigger and condition values
Each automation MUST persist a `debug` boolean (default false when omitted). When `debug` is true and the host evaluates that automation for a listening trigger or TIMER finish, it MUST emit log lines that include the values used for listener/trigger matching and for each comparison involved in condition evaluation (or note that condition enable is false). Those lines MUST appear in the same firmware log path operators read from System → Log.

#### Scenario: Debug on condition compare
- **WHEN** Debug is true, a listening device changes, and a comparison reads a live status value
- **THEN** the log includes that automation’s name and the compared live value (and compare target) for that evaluation

#### Scenario: Debug off stays quiet
- **WHEN** Debug is false and a listening device changes for that automation
- **THEN** the host does not emit those per-value debug lines for that evaluation

## MODIFIED Requirements

### Requirement: Automations persist on the host
The host MUST persist automations in its store. Each automation MUST have a unique `name`, an optional `title` (MAY be empty; not required for new edits), an optional `group` string, an `active` flag, a condition-enable flag, a `debug` flag, a TIMER complex attribute (persisted fields), a listening-devices list, a root condition (MAY be empty when TIMER `use` is true or when condition enable is false), and an ordered actions list. After reboot the same definitions MUST load. The slave MUST NOT store or evaluate automations. Runtime TIMER fields `counting` and `timeLeft` MUST NOT be persisted as settings. An omitted or empty `group` MUST be treated as ungrouped (UI label **Not in group**).

#### Scenario: Survives host reboot
- **WHEN** an automation is saved with a group and the host reboots
- **THEN** that automation’s name, group, active, condition-enable, debug, TIMER persisted fields, listeners, condition, and actions are still present

#### Scenario: Empty group is ungrouped
- **WHEN** an automation is saved with an empty `group`
- **THEN** the host stores it as ungrouped and list/filter UIs place it under **Not in group**

#### Scenario: Timer runtime not persisted
- **WHEN** an automation is counting down and the host reboots
- **THEN** `counting` and `timeLeft` are not restored from settings (countdown is not running until started again)

### Requirement: Timer-only and non-timer validation
When TIMER `use` is false, the automation MUST have at least one listening device and, when condition enable is true, at least one condition; the host MUST reject saves that violate this. When TIMER `use` is true, or when condition enable is false, the automation MAY have no conditions; an empty or disabled condition MUST evaluate as true so the automation can run when triggered. When TIMER `use` is true, the automation MAY have no listening devices.

#### Scenario: Timer-only automation
- **WHEN** an active automation has TIMER `use` true, no listeners, no conditions, and the countdown finishes
- **THEN** the host treats conditions as true and runs its actions

#### Scenario: Non-timer requires listen and condition
- **WHEN** the operator saves an automation with TIMER `use` false, condition enable true, and empty listeners
- **THEN** the host rejects the save

#### Scenario: Condition disabled without tree
- **WHEN** the operator saves an automation with condition enable false and an empty condition tree
- **THEN** the host accepts the save if other validation rules pass
