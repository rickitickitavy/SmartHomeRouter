# Spec Delta

## ADDED Requirements

### Requirement: Automation Trigger rows use typed parameters
Each Trigger (listening) row MUST let the operator choose a registered device, a channel when that device has more than one channel (channel control MUST be hidden when only one channel is available), and either a **known parameter** for that device’s classified type or **CUSTOM**. When CUSTOM is selected, the row MUST show cluster and attrId fields for entry. The Remove control on Trigger rows MUST use the same compact button sizing as Remove controls in Condition and Actions (`btn-inline` / shared editor button style).

#### Scenario: Hide single-channel picker
- **WHEN** the operator selects a device that has only one channel on a Trigger row
- **THEN** no channel control is shown for that row

#### Scenario: Custom shows cluster and attrId
- **WHEN** the operator chooses CUSTOM on a Trigger row
- **THEN** cluster and attrId inputs are visible and required for a complete custom binding

#### Scenario: Trigger Remove matches other editors
- **WHEN** the operator views Trigger, Condition, and Actions Remove buttons
- **THEN** Trigger Remove uses the same compact inline button styling as the other Remove buttons

### Requirement: Automation Condition comparisons use typed parameters
Each condition comparison MUST use the same device / channel / known-parameter-or-CUSTOM picker as Trigger. For a known parameter, the compare value MUST be chosen or entered against the host’s interpreted status vocabulary for that parameter. For CUSTOM, compare ops MUST apply to the numeric raw attribute value **VAL** (not the interpreted status string).

#### Scenario: Custom condition uses VAL
- **WHEN** the operator saves a CUSTOM condition comparison with EQUAL and value `42`
- **THEN** evaluation compares the numeric attribute VAL to 42

#### Scenario: Known On/Off status compare
- **WHEN** the operator picks an On/Off device’s Status parameter and EQUAL to ON
- **THEN** the dialog stores a known Status binding that compares the interpreted ON/OFF status

### Requirement: Automation device actions use typed parameters
Each device-target action MUST use the same device / channel / known-parameter-or-CUSTOM picker pattern as Trigger. Known parameters MUST map to the existing device-command payloads for that type (for example On/Off ON/OFF, window covering open/close/stop/position). When CUSTOM is selected, the row MUST show cluster, command, and value fields.

#### Scenario: Custom action fields
- **WHEN** the operator chooses CUSTOM on a device action
- **THEN** cluster, command, and value inputs are visible

#### Scenario: Known dimmer light level
- **WHEN** the operator picks Light level on a level-capable device action
- **THEN** the dialog collects the level value and stores a known Light-level action binding

### Requirement: Known parameter lists by device type
The console MUST offer known parameters based on the device’s classified type (and related clusters when present), including at least: Window covering — Status (STOPPED, MOVING UP, MOVING DOWN) and Position; On/Off — Status (ON, OFF); Level/dimmer — Status (ON, OFF) and Light level; measurement sensors — Sensor value (and Battery level when the device exposes battery); IAS zone — zone Status (alarm/clear style strings the host already publishes). Devices with no matching catalog entry MUST still offer CUSTOM.

#### Scenario: Window covering known list
- **WHEN** the operator selects a window-covering device on Trigger or Condition
- **THEN** the parameter list includes Status and Position plus CUSTOM

## MODIFIED Requirements

### Requirement: Automation edit dialog import and export
Add or row edit MUST open an edit dialog with **General** (Active, then Name, then Group, Debug; no title), **Trigger** (countdown TIMER controls first, then listening devices with typed parameter pickers), Condition (enable checkbox plus editor when enabled, with typed comparison pickers), actions (device actions with typed pickers or automation targets), Export, and Import. Dialog Import MUST be enabled only when the form is empty (no listeners, no actions, condition disabled or empty, TIMER `use` false / defaults unset). Dialog Export MUST download one JSON file named from the automation’s `name`.

#### Scenario: Group after name
- **WHEN** the operator opens the automation edit dialog
- **THEN** the Group field appears immediately below Name and accepts typed text or a choice from existing groups

#### Scenario: Empty-only dialog import
- **WHEN** the edit dialog already has a listener, action, or TIMER `use` true
- **THEN** Import is disabled

#### Scenario: No Title field
- **WHEN** the operator opens the automation edit dialog
- **THEN** there is no Title input
