# automations Specification

## Purpose

Host-side automations that watch selected devices for status value changes and/or an optional TIMER countdown, evaluate nested condition trees, and run device commands or automation control actions when those conditions are true.

## Requirements

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

### Requirement: Listen entry event type Change or Any value

Each listening entry MUST persist an event type of `change` or `any`. The UI MUST present these as **Change** and **Any value**. When the field is omitted on load, the host MUST treat it as `change`. New listen rows MUST default to `change`.

#### Scenario: Default change

- **WHEN** an older automation JSON has a listener without `eventType`
- **THEN** the host treats that listener as `change`

#### Scenario: Persist any

- **WHEN** the operator saves a listener with Event type **Any value**
- **THEN** export/reload still has `eventType` `any` for that listener

### Requirement: Event type gates same-value status triggers

For a listening entry with event type `change`, the host MUST NOT start status-path evaluation for that listener when the bound status value is equal to the previously stored value for that scope. For event type `any`, the host MUST still allow evaluation for that listener on a matching status report even when the value is unchanged (including discrete ON/OFF, LEAK/DRY, covering move). TIMER expiry and current-time polling are unchanged by this field.

#### Scenario: Change ignores same DRY

- **WHEN** a listener has event type `change`, param status, and the IAS Zone device re-reports DRY equal to the previous status
- **THEN** that automation is not evaluated for that report via that listener

#### Scenario: Any value accepts same DRY

- **WHEN** a listener has event type `any`, param status, and the IAS Zone device re-reports DRY equal to the previous status
- **THEN** the host still evaluates that automation for that report

### Requirement: Listening devices gate status-change evaluation

Each listening entry MUST identify a registered device and MAY specify a cluster, a known parameter, and/or an attribute id. The host MUST evaluate an active automation on a status-change trigger only when a listening device’s relevant status matches the bound scope (parameter/attr when set, else cluster when set, else any status for that device) **and** that listener’s event type allows the report (`change` requires a value change; `any` allows same-value re-reports). The previous blanket exception that always re-notified discrete statuses for every listener is replaced by per-listener event type. If a listener has a cluster (and no tighter attr/param), only changes for that cluster MUST trigger that automation via the status path. TIMER expiry MUST still evaluate when `use` is true even if no listener matched.

#### Scenario: Same value ignored under Change

- **WHEN** a listening device reports a status equal to its current status and the matching listener event type is `change`
- **THEN** no automation condition check runs for that report via that listener

#### Scenario: Discrete same value under Any value

- **WHEN** a listening IAS Zone device re-reports DRY equal to the previous status and the matching listener event type is `any`
- **THEN** the host still evaluates that automation

#### Scenario: Cluster filter

- **WHEN** a listener specifies cluster X and the device changes status for cluster Y
- **THEN** that automation is not evaluated for that change via the status path

#### Scenario: Any status when unbound

- **WHEN** a listener has no cluster and no param/attrId and any status value for that device changes (event type `change`)
- **THEN** the host evaluates that active automation’s condition

### Requirement: Condition trees use comparisons and containers
A condition MUST be a comparison or a container. Every condition MUST have a NOT flag. A container MUST have concatenation type AND, OR, or XOR and an ordered list of child conditions (comparisons or containers). A comparison MUST identify device, channel when the device is multichannel, optional cluster, optional known parameter or attrId, compare type (EQUAL, GREATER, LESS, CONTAINS, STARTS, ENDS), and compare value. When comparison uses a known parameter, the host MUST resolve that parameter’s interpreted status. When comparison is CUSTOM, GREATER/LESS MUST compare numeric VAL; EQUAL MAY compare interpreted status text (for example DRY) or numeric VAL when both sides parse as numbers. When comparison omits typed parameter and cluster, the host MUST use the device’s default main status.

#### Scenario: Nested AND
- **WHEN** a container AND has two child comparisons both true
- **THEN** the container is true

#### Scenario: NOT inverts
- **WHEN** a comparison would be true and NOT is set
- **THEN** the condition is false

#### Scenario: Default main status
- **WHEN** a comparison omits cluster/param and the device’s default main status matches EQUAL to the compare value
- **THEN** the comparison is true

### Requirement: Typed listen bindings
Each listening entry MUST identify a registered device, a channel when multi-channel, and either a known parameter key or a custom cluster plus attribute id. When a known parameter or custom attrId is set, the host MUST evaluate that automation on a status-change trigger only when that specific parameter/attribute event changes for the bound device (and channel). A change to a different attribute on the same device MUST NOT trigger that listener. Omitted typed fields on older JSON MUST keep prior cluster-only (or any-status) matching for compatibility.

#### Scenario: Specific attribute listen
- **WHEN** a listener is bound to window-covering Position and only Move Status changes
- **THEN** that automation is not evaluated for that change via the status path

#### Scenario: Custom attrId listen
- **WHEN** a listener uses CUSTOM cluster `0x0006` and attrId `0x0000` and that OnOff attribute reports a new value
- **THEN** the host evaluates that active automation’s condition

#### Scenario: Legacy listener without param
- **WHEN** an older automation JSON has a listener with ieee and optional cluster but no param/attrId
- **THEN** the host keeps the previous cluster-or-any matching behavior for that listener

### Requirement: Typed condition comparisons and custom VAL
A comparison MUST identify device, channel when multichannel, and either a known parameter or CUSTOM cluster plus attrId, plus compare type and compare value. Known-parameter comparisons MUST use the host’s interpreted status text for that parameter. CUSTOM GREATER/LESS MUST parse numeric VAL; CUSTOM EQUAL MUST accept a non-empty compare value and MAY compare interpreted status text or numeric VAL.

#### Scenario: Custom VAL greater
- **WHEN** a CUSTOM comparison has GREATER and value `100` and the live attribute VAL is `150`
- **THEN** that comparison evaluates true

#### Scenario: Custom EQUAL to interpreted status
- **WHEN** a CUSTOM comparison has EQUAL and value `DRY` and the live status is DRY
- **THEN** that comparison evaluates true

#### Scenario: Known Status equal
- **WHEN** a known On/Off Status comparison has EQUAL and value `ON` and the live status is ON
- **THEN** that comparison evaluates true

### Requirement: Typed device actions including custom command
A device-target action MUST identify device, channel when multichannel, and either a known parameter/command binding (with any required value such as light level or position) or CUSTOM with cluster, command id, and value. Known bindings MUST execute through the existing device-command path with the mapped payload. CUSTOM MUST send the specified cluster command and value to that device/channel. Automation-target actions are unchanged.

#### Scenario: Custom cluster command runs
- **WHEN** actions run a CUSTOM device action with cluster, command, and value
- **THEN** the host issues that command to the bound device channel

#### Scenario: Known On/Off action
- **WHEN** actions run a known On/Off Status action with ON
- **THEN** the host sends the same On command path as a console On payload for that device

### Requirement: True condition runs actions

When a status-change or TIMER-finish trigger finds an active automation’s root condition true, the host MUST run its actions in order. When the status-path evaluation was started because a matching listener with event type `any` accepted the report, the host MUST run actions even if the condition was already true on the previous evaluation (subject to `minTriggerIntervalSec`). When evaluation was started only under event type `change` (a real value change), existing condition evaluation applies; current-time condition polling MAY continue to require a false→true edge. Inactive automations MUST NOT run. A false condition MUST NOT run actions.

#### Scenario: Actions fire in order

- **WHEN** a listening change makes the condition true and the automation has two actions
- **THEN** the host performs the first action then the second

#### Scenario: Any value re-report runs while condition stays true

- **WHEN** a listener with event type `any` matches a status report, the root condition evaluates true, and the previous evaluation also left the condition true
- **THEN** the host still runs the automation’s actions (subject to `minTriggerIntervalSec`)

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
The Automations list MUST support Export all and Import all as JSON of the persisted automation definitions (no last-activation or TIMER runtime telemetry). Import all MUST **merge** into the stored set from a valid file: each imported automation whose `name` already exists MUST replace that automation; each imported automation whose `name` is new MUST be added; automations already on the host whose `name` does **not** appear in the import file MUST remain unchanged. Import all MUST NOT clear the store before applying the file. Export all behavior is unchanged.

#### Scenario: Export all then import all
- **WHEN** the operator exports all automations and later imports that file
- **THEN** the host automations match the file definitions for those names

#### Scenario: Import upserts by name
- **WHEN** Import all includes an automation whose `name` already exists
- **THEN** that automation is replaced by the imported definition

#### Scenario: Import keeps locals not in file
- **WHEN** the host has automations `A` and `B`, and Import all contains only `A` (updated)
- **THEN** `A` is updated from the file and `B` remains on the host

### Requirement: Automation groups for list filtering
The host MUST expose each automation’s `group` in list/summary JSON. Distinct non-empty group strings MUST be available so the UI can build a group list. Automations with empty `group` MUST appear only under the ungrouped bucket (**Not in group**), not under named groups.

#### Scenario: List includes group
- **WHEN** the operator requests the automations list and an automation has `group` `Lights`
- **THEN** that row’s JSON includes `"group":"Lights"`

#### Scenario: Ungrouped bucket
- **WHEN** an automation has empty `group`
- **THEN** it is not reported under any named group value and the UI treats it as **Not in group**

### Requirement: Device actions offer registered custom commands
When an automation device-target action selects a registered device, the command dropdown MUST include that device’s saved custom commands in addition to existing known bindings and ZCL CUSTOM. Selecting a custom command MUST identify that command by name on the action. Existing ZCL CUSTOM (cluster/command id/value) behavior MUST remain unchanged.

#### Scenario: Custom command listed
- **WHEN** device A has custom command `reboot` and the operator opens a device action for A
- **THEN** the command dropdown includes `reboot`

#### Scenario: Other device commands not listed
- **WHEN** device A has custom command `reboot` and the operator opens a device action for device B that has no custom commands
- **THEN** the command dropdown for B does not include `reboot`

### Requirement: Custom command action requires parameter value when needed
When the selected custom command requires a parameter, the automation action editor MUST show a value control typed for that parameter (`int`, `bool` ON/OFF, `float` with two decimals, or `string`). The value MUST be non-empty and valid for the type before the automation can be saved. When the command has no parameter, the editor MUST NOT require a value field for that custom command.

#### Scenario: Missing bool value rejected
- **WHEN** the operator selects a custom command that requires bool and leaves the value empty
- **THEN** the host rejects the automation save

#### Scenario: Filled int value accepted
- **WHEN** the operator selects a custom command that requires int, enters `25`, and saves
- **THEN** the automation stores that action with the custom command name and value `25`

### Requirement: Running a custom command action executes device format
When actions run a device-target action bound to a registered custom command, the host MUST execute that command on the bound device and channel using the MQTT or Zigbee custom-command payload format for that device’s transport, with the stored parameter value when required.

#### Scenario: MQTT custom action fires
- **WHEN** actions run a custom command `level=40` on an MQTT device
- **THEN** the host publishes `level=40` on that device’s command topic

#### Scenario: Zigbee custom action fires
- **WHEN** actions run a no-parameter custom command `identify` on a Zigbee device
- **THEN** the host issues body `cmd=identify` on that device’s command path
