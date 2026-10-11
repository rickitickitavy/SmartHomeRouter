# Spec Delta: web-console

## MODIFIED Requirements

### Requirement: Automation section lists automations
The Automation section MUST show a narrow left group list and a table of automations. The group list MUST start with **All**, then distinct non-empty groups, and include **Not in group** when any ungrouped automation exists (or always as a selectable bucket). Selecting **All** MUST show every automation. Selecting a named group MUST show only automations with that `group`. Selecting **Not in group** MUST show only automations with empty `group`. The table MUST show at least name, active, and last performed when known, plus timer columns already present. Buttons Add, Export all, and Import all remain. Only `isAdmin` MUST see Automation.

#### Scenario: Filter by group
- **WHEN** an `isAdmin` selects group `Lights` and two automations have that group
- **THEN** the table shows only those two rows

#### Scenario: All shows everything
- **WHEN** an `isAdmin` selects **All**
- **THEN** the table includes automations from every group and ungrouped ones

#### Scenario: Open Automation list
- **WHEN** an `isAdmin` opens Automation and at least one automation is stored
- **THEN** each visible row shows name, active, and last performed when known

### Requirement: Automation edit dialog import and export
Add or row edit MUST open an edit dialog with name, **group** (immediately after name; free text with dropdown of existing groups), title, active, TIMER (`use`, `startOnBoot`, `autoRestart`, `defaultTime`, and runtime `counting` / `timeLeft` when known), listening devices, condition editor, actions (device or automation targets), Export, and Import. Dialog Import MUST be enabled only when the form is empty (no listeners, no actions, empty condition, TIMER `use` false / defaults unset). Dialog Export MUST download one JSON file named from the automation’s `name`.

#### Scenario: Group after name
- **WHEN** the operator opens the automation edit dialog
- **THEN** the Group field appears immediately below Name and accepts typed text or a choice from existing groups

#### Scenario: Empty-only dialog import
- **WHEN** the edit dialog already has a listener, action, or TIMER `use` true
- **THEN** Import is disabled

## ADDED Requirements

### Requirement: Status Memory shows free RAM and PSRAM
The Status Memory card MUST be titled to reflect free memory (for example **Free memory** or group title **Memory** with row labels **Free RAM** / **Free PSRAM**). Values MUST show free heap/PSRAM in kB and free percent of total (not used amounts). When PSRAM is absent, PSRAM MUST show as unavailable (em dash or equivalent), not zero used.

#### Scenario: Free heap displayed
- **WHEN** the operator opens Status and the host reports free heap
- **THEN** the Memory card shows free RAM in kB and free percent, not used heap

#### Scenario: Free PSRAM when present
- **WHEN** PSRAM is present
- **THEN** the Memory card shows free PSRAM in kB and free percent

### Requirement: Console editor can bind automations
In the Consoles edit dialog, adding a binding MUST allow choosing a registered device or an automation. Automation bindings MUST appear as On/Off rows in the editor preview (display only; no active change until Save + user view). Saved layouts MUST round-trip automation bindings through export/import and settings restore.

#### Scenario: Add automation binding
- **WHEN** the editor adds automation `yard-light` to a widget and saves
- **THEN** the console JSON includes that automation binding and the user page shows its On/Off row
