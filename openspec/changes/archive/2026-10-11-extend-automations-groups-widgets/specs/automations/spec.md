# Spec Delta: automations

## MODIFIED Requirements

### Requirement: Automations persist on the host
The host MUST persist automations in its store. Each automation MUST have a unique `name`, a `title`, an optional `group` string, an `active` flag, a TIMER complex attribute (persisted fields), a listening-devices list, a root condition (MAY be empty when TIMER `use` is true), and an ordered actions list. After reboot the same definitions MUST load. The slave MUST NOT store or evaluate automations. Runtime TIMER fields `counting` and `timeLeft` MUST NOT be persisted as settings. An omitted or empty `group` MUST be treated as ungrouped (UI label **Not in group**).

#### Scenario: Survives host reboot
- **WHEN** an automation is saved with a group and the host reboots
- **THEN** that automation’s name, title, group, active, TIMER persisted fields, listeners, condition, and actions are still present

#### Scenario: Empty group is ungrouped
- **WHEN** an automation is saved with an empty `group`
- **THEN** the host stores it as ungrouped and list/filter UIs place it under **Not in group**

#### Scenario: Timer runtime not persisted
- **WHEN** an automation is counting down and the host reboots
- **THEN** `counting` and `timeLeft` are not restored from settings (countdown is not running until started again)

### Requirement: TIMER attribute
Each automation MUST expose TIMER with: `use`, `counting`, `timeLeft` (seconds), `autoRestart`, `startOnBoot`, and `defaultTime`. `use`, `autoRestart`, `startOnBoot`, and `defaultTime` are settings. `counting` and `timeLeft` are runtime-only. When `use` is true, countdown reaching zero MUST be a trigger that evaluates conditions. When `active` changes from false to true and `use` is true and `autoRestart` is true and `defaultTime` is set, the host MUST load `defaultTime` into `timeLeft` and start countdown (`counting` true). When `active` changes from false to true and `use` is true and `startOnBoot` is true and `defaultTime` is set, the host MUST likewise start countdown from `defaultTime` (even if `autoRestart` is false). After a countdown finishes and evaluation is attempted, if `autoRestart` is true and `defaultTime` is set, the host MUST reload `defaultTime` and start countdown again. When `use` and `startOnBoot` are true, the host MUST still start eligible timers once ~20 seconds after boot as today.

#### Scenario: Timer expiry triggers evaluation
- **WHEN** an active automation has TIMER `use` true, is counting, and `timeLeft` reaches zero
- **THEN** the host evaluates that automation’s conditions

#### Scenario: Active edge starts timer via startOnBoot
- **WHEN** an automation with TIMER `use` true, `startOnBoot` true, `autoRestart` false, and `defaultTime` 40 becomes active
- **THEN** countdown starts with 40 seconds remaining

#### Scenario: Active edge starts timer via autoRestart
- **WHEN** an automation with TIMER `use` true, `autoRestart` true, and `defaultTime` 60 becomes active
- **THEN** countdown starts with 60 seconds remaining

#### Scenario: Auto restart after finish
- **WHEN** countdown finishes for an automation with `autoRestart` true and `defaultTime` 30
- **THEN** after evaluation is attempted the countdown starts again with 30 seconds

## ADDED Requirements

### Requirement: Automation groups for list filtering
The host MUST expose each automation’s `group` in list/summary JSON. Distinct non-empty group strings MUST be available so the UI can build a group list. Automations with empty `group` MUST appear only under the ungrouped bucket (**Not in group**), not under named groups.

#### Scenario: List includes group
- **WHEN** the operator requests the automations list and an automation has `group` `Lights`
- **THEN** that row’s JSON includes `"group":"Lights"`

#### Scenario: Ungrouped bucket
- **WHEN** an automation has empty `group`
- **THEN** it is not reported under any named group value and the UI treats it as **Not in group**
