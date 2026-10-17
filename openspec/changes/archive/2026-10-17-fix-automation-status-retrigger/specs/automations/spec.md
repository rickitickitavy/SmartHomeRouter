# Spec Delta

## ADDED Requirements

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

## MODIFIED Requirements

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
