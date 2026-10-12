# Spec Delta

## ADDED Requirements

### Requirement: Hardware tab keeps Save and Reset visible
System → Hardware MUST scroll its settings fields when content exceeds the panel height and MUST keep the Save and Reset controls visible at the bottom of the Hardware panel (not scrolled out of view with the fields).

#### Scenario: Short viewport Hardware
- **WHEN** an admin opens System → Hardware on a viewport shorter than the Hardware fields
- **THEN** the fields scroll inside the panel and Save and Reset remain visible at the bottom

### Requirement: Automation editor uses spaced groups including General
The automation edit dialog body MUST present groups in this order with about one blank line of vertical space between them: **General**, **Trigger**, **Condition**, **Actions**. General MUST be a single container holding Active, Name, Group, then optional Min trigger interval if still shown, then Debug—without wrapping each of those controls in its own field card. The Trigger group MUST be a single container that starts with the countdown timer controls (enable labeled **Use countdown timer**), then **Triggered by devices** (heading, trigger-device rows, and **Add device to trigger**). The dialog MUST NOT show a Title field.

#### Scenario: General order without Title
- **WHEN** the operator opens the automation edit dialog
- **THEN** General shows Active, then Name, then Group, and no Title control appears

#### Scenario: Group spacing
- **WHEN** the operator views the edit dialog with Trigger and Condition sections present
- **THEN** General, Trigger, Condition, and Actions are separated by visible vertical spacing

#### Scenario: Timer at start of Trigger
- **WHEN** the operator views the Trigger group
- **THEN** Use countdown timer appears before Triggered by devices inside the same container

### Requirement: Automation condition group has an enable checkbox
The Condition group MUST include a checkbox that enables the main condition editor. When the checkbox is unchecked, the main condition editor MUST be hidden. That checkbox state MUST persist with the automation and round-trip through dialog Save, export/import, and settings restore.

#### Scenario: Hide condition when disabled
- **WHEN** the operator unchecks the Condition enable control
- **THEN** the condition tree editor is hidden

#### Scenario: Condition enable survives save
- **WHEN** the operator disables Condition, saves, and reopens the automation
- **THEN** Condition remains disabled and the editor stays hidden

### Requirement: Automation General includes Debug
General MUST end with a Debug checkbox. That flag MUST persist with the automation and round-trip through Save, export/import, and settings restore. Runtime debug behavior is defined by the automations capability.

#### Scenario: Debug at end of General
- **WHEN** the operator opens the automation edit dialog
- **THEN** Debug is the last control in the General group

## MODIFIED Requirements

### Requirement: Automation edit dialog import and export
Add or row edit MUST open an edit dialog with **General** (Active, then Name, then Group, Debug; no title), **Trigger** (countdown TIMER controls first, then listening devices), Condition (enable checkbox plus editor when enabled), actions (device or automation targets), Export, and Import. Dialog Import MUST be enabled only when the form is empty (no listeners, no actions, condition disabled or empty, TIMER `use` false / defaults unset). Dialog Export MUST download one JSON file named from the automation’s `name`.

#### Scenario: Group after name
- **WHEN** the operator opens the automation edit dialog
- **THEN** the Group field appears immediately below Name and accepts typed text or a choice from existing groups

#### Scenario: Empty-only dialog import
- **WHEN** the edit dialog already has a listener, action, or TIMER `use` true
- **THEN** Import is disabled

#### Scenario: No Title field
- **WHEN** the operator opens the automation edit dialog
- **THEN** there is no Title input
