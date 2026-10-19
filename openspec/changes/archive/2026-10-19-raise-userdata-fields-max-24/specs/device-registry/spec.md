# Spec Delta

## MODIFIED Requirements

### Requirement: User-data devices store ordered field schema

Each `userData` device SHALL persist ordered `userDataFields` with `nullable`, unique non-empty `name`, and `type` (`int`/`float`/`bool`/`string`). List order is attr `0x0001` value-slot order. Omitted fields on create store `[]`. Schema MUST round-trip in `devices.json`, list JSON, and device export/import. A device MUST accept at most **24** fields; attempts to store more MUST be rejected or truncated consistently so the persisted count never exceeds 24.

#### Scenario: Save schema then reload

- **WHEN** the operator saves a `userData` device with two fields (nullable off, name `temp`, type `float`) then (nullable on, name `label`, type `string`)
- **THEN** after reboot list JSON for that IEEE includes those two fields in that order with the same nullable, name, and type

#### Scenario: Empty schema allowed

- **WHEN** the operator saves a new `userData` device with no fields
- **THEN** the record has type `userData` and an empty `userDataFields` list

#### Scenario: Twenty-four fields persist

- **WHEN** the operator saves a `userData` device with 24 uniquely named fields
- **THEN** after reload list JSON includes all 24 fields in the saved order
