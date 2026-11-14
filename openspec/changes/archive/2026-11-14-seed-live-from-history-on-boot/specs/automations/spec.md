# Spec Delta

## ADDED Requirements

### Requirement: History-seeded live values do not trigger status path
When the host copies a last history sample into the runtime live-value cache after boot, that update SHALL NOT start status-path automation evaluation and SHALL NOT enqueue automation actions. Only a later real status report (or TIMER expiry / other existing non-seed triggers) MAY evaluate automations under existing listener and event-type rules.

#### Scenario: Seed alone does not enqueue
- **WHEN** boot seeds a listening device’s attribute from history and an active automation would match that attribute on a real change
- **THEN** the host does not enqueue that automation’s actions solely because of the seed

#### Scenario: Real report after seed still can trigger
- **WHEN** the same attribute receives a real status report after a history seed and a matching listener’s event type allows it
- **THEN** the host evaluates that automation under existing rules
