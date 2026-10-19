# Spec Delta

## MODIFIED Requirements

### Requirement: User-data parameter dialog edits field schema

For type `userData`, the parameter dialog MUST show a User data fields table (columns: nullable checkbox, name, type) with per-row Remove, Up, and Down. Add is draft-only until Save; Cancel discards. Types are `int`, `float`, `bool`, `string`. Hide the table for other types. The dialog MUST allow up to **24** fields (Add MUST NOT append a 25th row).

#### Scenario: Reorder then save

- **WHEN** the operator edits a `userData` device, moves the second field above the first, and Saves
- **THEN** after reload the schema list order matches the reordered table

#### Scenario: Cancel discards draft rows

- **WHEN** the operator Adds a field row in the dialog and Cancels without Save
- **THEN** after reopen the stored schema does not include that unsaved row

#### Scenario: Hidden for On/Off

- **WHEN** the operator opens parameters for an `onOff` device
- **THEN** the User data fields table is not shown

#### Scenario: Add stops at twenty-four

- **WHEN** the operator already has 24 field rows in the User data fields table and activates Add field
- **THEN** the table still has 24 rows (no 25th row is added)
