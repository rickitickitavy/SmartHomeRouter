# Spec Delta

## MODIFIED Requirements

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
