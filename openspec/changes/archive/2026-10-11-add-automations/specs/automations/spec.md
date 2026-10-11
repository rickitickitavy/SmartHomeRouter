# Spec Delta

## Purpose

Host-side automations that watch selected devices for status value changes and/or an optional TIMER countdown, evaluate nested condition trees, and run device commands or automation control actions when those conditions are true.

## ADDED Requirements

### Requirement: Automations persist on the host
The host MUST persist automations in its store. Each automation MUST have a unique `name`, a `title`, an `active` flag, a TIMER complex attribute (persisted fields), a listening-devices list, a root condition (MAY be empty when TIMER `use` is true), and an ordered actions list. After reboot the same definitions MUST load. The slave MUST NOT store or evaluate automations. Runtime TIMER fields `counting` and `timeLeft` MUST NOT be persisted as settings.

#### Scenario: Survives host reboot
- **WHEN** an automation is saved and the host reboots
- **THEN** that automation’s name, title, active, TIMER persisted fields, listeners, condition, and actions are still present

#### Scenario: Slave has no copy
- **WHEN** the host has saved automations and the slave reboots alone
- **THEN** automations remain only on the host and the slave does not evaluate them

#### Scenario: Timer runtime not persisted
- **WHEN** an automation is counting down and the host reboots
- **THEN** `counting` and `timeLeft` are not restored from settings (countdown is not running until started again)

### Requirement: TIMER attribute
Each automation MUST expose TIMER with: `use`, `counting`, `timeLeft` (seconds), `autoRestart`, and `defaultTime`. `use`, `autoRestart`, and `defaultTime` are settings. `counting` and `timeLeft` are runtime-only. When `use` is true, countdown reaching zero MUST be a trigger that evaluates conditions. When `active` changes from false to true and `use` is true and `autoRestart` is true and `defaultTime` is set, the host MUST load `defaultTime` into `timeLeft` and start countdown (`counting` true). After a countdown finishes and evaluation is attempted, if `autoRestart` is true and `defaultTime` is set, the host MUST reload `defaultTime` and start countdown again.

#### Scenario: Timer expiry triggers evaluation
- **WHEN** an active automation has TIMER `use` true, is counting, and `timeLeft` reaches zero
- **THEN** the host evaluates that automation’s conditions

#### Scenario: Active edge starts timer
- **WHEN** an automation with TIMER `use` true, `autoRestart` true, and `defaultTime` 60 becomes active
- **THEN** countdown starts with 60 seconds remaining

#### Scenario: Auto restart after finish
- **WHEN** countdown finishes for an automation with `autoRestart` true and `defaultTime` 30
- **THEN** after evaluation is attempted the countdown starts again with 30 seconds

### Requirement: Timer-only and non-timer validation
When TIMER `use` is false, the automation MUST have at least one listening device and at least one condition; the host MUST reject saves that violate this. When TIMER `use` is true, the automation MAY have no listening devices and no conditions; an empty condition tree MUST evaluate as true so the automation runs when the timer finishes.

#### Scenario: Timer-only automation
- **WHEN** an active automation has TIMER `use` true, no listeners, no conditions, and the countdown finishes
- **THEN** the host treats conditions as true and runs its actions

#### Scenario: Non-timer requires listen and condition
- **WHEN** the operator saves an automation with TIMER `use` false and empty listeners
- **THEN** the host rejects the save

### Requirement: Listening devices gate status-change evaluation
Each listening entry MUST identify a registered device and MAY specify a cluster. The host MUST evaluate an active automation on a status-change trigger only when a listening device’s relevant status value changes. A status update with the same value as the previous stored status MUST NOT trigger evaluation. If a listener has a cluster, only changes for that cluster MUST trigger that automation via the status path. TIMER expiry MUST still evaluate when `use` is true even if no listener matched.

#### Scenario: Same value ignored
- **WHEN** a listening device reports a status equal to its current status
- **THEN** no automation condition check runs for that report

#### Scenario: Cluster filter
- **WHEN** a listener specifies cluster X and the device changes status for cluster Y
- **THEN** that automation is not evaluated for that change via the status path

#### Scenario: Unfiltered listener
- **WHEN** a listener has no cluster and any status value for that device changes
- **THEN** the host evaluates that active automation’s condition

### Requirement: Condition trees use comparisons and containers
A condition MUST be a comparison or a container. Every condition MUST have a NOT flag. A container MUST have concatenation type AND, OR, or XOR and an ordered list of child conditions (comparisons or containers). A comparison MUST identify device, channel when the device is multichannel, optional cluster, compare type (EQUAL, GREATER, LESS, CONTAINS, STARTS, ENDS), and compare value. When comparison cluster is omitted, the host MUST use the device’s default main status.

#### Scenario: Nested AND with NOT
- **WHEN** a container AND has a child comparison that is true and another child with NOT that would be true without NOT
- **THEN** the container evaluates false

#### Scenario: XOR container
- **WHEN** a XOR container has exactly one true child among two
- **THEN** the container evaluates true

#### Scenario: Default main status
- **WHEN** a comparison omits cluster and the device’s default main status matches EQUAL to the compare value
- **THEN** that comparison evaluates true

### Requirement: True condition runs actions
When a status-change or TIMER-finish trigger finds an active automation’s root condition true, the host MUST run its actions in order. Each action MUST target either a device (same device-command model as the console command API: device, payload, channel) or an automation. Inactive automations MUST NOT run. A false condition MUST NOT run actions.

#### Scenario: Actions fire in order
- **WHEN** a listening change makes the condition true and the automation has two actions
- **THEN** the host performs the first action then the second

#### Scenario: Inactive skipped
- **WHEN** a listening device status changes for an inactive automation whose condition would be true
- **THEN** no actions run for that automation

### Requirement: Automation-target actions
An action that targets an automation MUST support `ENABLE` and `DISABLE`. When the target automation has TIMER `use` true, that action MUST also support `STOP TIMER`, `START TIMER`, and `RESTART TIMER`. `START TIMER` MAY include a countdown time in seconds; when the target has no `defaultTime`, that optional time MUST be usable to start the countdown. Timer ops against a target with TIMER `use` false MUST NOT be accepted.

#### Scenario: Enable another automation
- **WHEN** an action with op ENABLE targets automation B and A’s condition is true
- **THEN** automation B becomes active

#### Scenario: Start timer with explicit time
- **WHEN** automation B uses a timer with no defaultTime and an action START TIMER supplies 45 seconds
- **THEN** B’s countdown starts with 45 seconds remaining

#### Scenario: Timer op rejected without timer use
- **WHEN** an action START TIMER targets an automation whose TIMER `use` is false
- **THEN** the host rejects that action on save or skips it as invalid at run time without applying a countdown

### Requirement: Last activation is runtime only
The host MUST track last-activation date and time in RAM for display. Last activation MUST NOT be part of persisted settings. After reboot with no activation yet, the UI MUST show that no last activation is known.

#### Scenario: Cleared after reboot
- **WHEN** an automation activated before reboot and the host reboots
- **THEN** last activation is unknown until the next successful activation

#### Scenario: Set on run
- **WHEN** actions run because a condition became true after a listening change or timer finish
- **THEN** last activation shows that run’s date and time

### Requirement: List import and export
The Automations list MUST support Export all and Import all as JSON of the persisted automation definitions (no last-activation or TIMER runtime telemetry). Import all MUST replace the stored set from a valid file. Name collisions in Import all MUST follow replace-by-name semantics (imported `name` overwrites the existing automation with that `name`).

#### Scenario: Export all then import all
- **WHEN** the operator exports all automations and later imports that file
- **THEN** the host automations match the file definitions

#### Scenario: Import replaces by name
- **WHEN** Import all includes an automation whose `name` already exists
- **THEN** that automation is replaced by the imported definition
