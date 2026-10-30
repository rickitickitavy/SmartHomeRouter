# Spec Delta

## MODIFIED Requirements

### Requirement: True condition runs actions

When a status-change or TIMER-finish trigger finds an active automation’s root condition true, the host MUST enqueue its actions in order on the action/command FIFO (see capability `action-command-queue`) rather than applying them inline on the trigger path. When the status-path evaluation was started because a matching listener with event type `any` accepted the report, the host MUST enqueue actions even if the condition was already true on the previous evaluation (subject to `minTriggerIntervalSec`). When evaluation was started only under event type `change` (a real value change), existing condition evaluation applies; current-time condition polling MAY continue to require a false→true edge. Inactive automations MUST NOT enqueue. A false condition MUST NOT enqueue actions. FIFO drain later applies each action (device, automation-control, or e-mail) with the existing semantics for that action type.

#### Scenario: Actions fire in order

- **WHEN** a listening change makes the condition true and the automation has two actions
- **THEN** the host enqueues the first action then the second, and the main-loop drain applies them in that FIFO order

#### Scenario: Any value re-report runs while condition stays true

- **WHEN** a listener with event type `any` matches a status report, the root condition evaluates true, and the previous evaluation also left the condition true
- **THEN** the host still enqueues the automation’s actions (subject to `minTriggerIntervalSec`)

#### Scenario: Inactive skipped

- **WHEN** a listening device status changes for an inactive automation whose condition would be true
- **THEN** no actions are enqueued for that automation

### Requirement: Operator-initiated Run from console widget

The host MUST accept an authenticated request that enqueues a named automation’s action list once on the action/command FIFO without requiring that automation’s condition to be true, subject to the existing nested-action depth limit. The HTTP handler MUST NOT apply those actions inline. Place and simple-user console pages MUST be able to invoke this when a widget automation binding control mode is Run.

#### Scenario: Run executes actions once

- **WHEN** an authenticated operator requests Run for automation B from a console widget
- **THEN** the host enqueues B’s actions once without requiring B’s condition to be true, and FIFO drain later applies them
