# Spec Delta

## ADDED Requirements

### Requirement: Operator-initiated Run from console widget
The host MUST accept an authenticated request that runs a named automation’s action list once without requiring that automation’s condition to be true, subject to the existing nested-action depth limit. Place and simple-user console pages MUST be able to invoke this when a widget automation binding control mode is Run.

#### Scenario: Run executes actions once
- **WHEN** an authenticated operator requests Run for automation B from a console widget
- **THEN** the host runs B’s actions once without requiring B’s condition to be true

### Requirement: Operator-initiated timer ops from console widget
When a console widget automation binding control mode is a timer op, the host MUST apply the same TIMER rules as automation-target timer actions: `START TIMER`, `STOP TIMER`, and `RESTART TIMER` only when the named automation has TIMER `use` true; otherwise the host MUST reject or no-op without changing countdown state.

#### Scenario: Start timer from widget
- **WHEN** automation B has TIMER `use` true and the operator activates Start timer on a widget bound to B
- **THEN** B’s countdown starts per existing START TIMER rules

### Requirement: Disabled automation rejects Run and timer start
When an automation is inactive (disabled), the host MUST NOT run its action list for an operator-initiated Run, and MUST NOT start or restart its timer. A countdown that was already stopped on disable MUST stay stopped.

#### Scenario: Run ignored when disabled
- **WHEN** automation B is inactive and an authenticated operator requests Run for B
- **THEN** the host does not run B’s actions

#### Scenario: Start timer ignored when disabled
- **WHEN** automation B is inactive, has TIMER `use` true, and the operator requests Start timer for B
- **THEN** B’s countdown does not start

