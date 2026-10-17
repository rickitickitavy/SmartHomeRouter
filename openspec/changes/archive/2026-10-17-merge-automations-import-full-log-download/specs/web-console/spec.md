# Spec Delta

## MODIFIED Requirements

### Requirement: Automation section lists automations

The Automation section MUST show a narrow left group list and a table of automations. The group list MUST start with **All**, then distinct non-empty groups, and include **Not in group** when any ungrouped automation exists (or always as a selectable bucket). Selecting **All** MUST show every automation. Selecting a named group MUST show only automations with that `group`. Selecting **Not in group** MUST show only automations with empty `group`. The table MUST show at least name, active, and last performed when known, plus timer columns already present. Buttons Add, Export all, and Import all remain. Import all MUST merge (upsert by `name`, keep automations not present in the file); the confirm dialog before Import all MUST state that merge behavior. Only `isAdmin` MUST see Automation.

#### Scenario: Filter by group

- **WHEN** an `isAdmin` selects group `Lights` and two automations have that group
- **THEN** the table shows only those two rows

#### Scenario: All shows everything

- **WHEN** an `isAdmin` selects **All**
- **THEN** the table includes automations from every group and ungrouped ones

#### Scenario: Open Automation list

- **WHEN** an `isAdmin` opens Automation and at least one automation is stored
- **THEN** each visible row shows name, active, and last performed when known

#### Scenario: Import all confirm mentions merge

- **WHEN** the operator chooses Import all and a confirm dialog is shown
- **THEN** the message indicates merge/upsert behavior and that existing automations not in the file are kept

### Requirement: Log tab shows recent firmware logs

The Log tab MUST display recent firmware log lines that are also emitted on USB when console logging is enabled. The page MUST be able to refresh that list without leaving the Log tab. Older lines MAY drop when the in-memory buffer is full. The Refresh control MUST stay visible without a second page scroller hiding it, MUST be left-aligned, and MUST use a compact content width (not a full-width bar). The Log card MUST NOT repeat a second “Log” title under the Log tab. The on-screen list MAY show only a bounded tail (on the order of 64 KiB) for UI performance. A **Download full log** control MUST download the entire current ring used length (up to ring capacity, typically the PSRAM ring), not only that on-screen tail.

#### Scenario: New log line appears

- **WHEN** the firmware emits a log line and the operator refreshes the Log tab (automatically or manually)
- **THEN** that line appears in the log view (unless it has already aged out of the buffer)

#### Scenario: USB logging still works

- **WHEN** a log line is emitted
- **THEN** it still appears on the USB serial console when USB debug logging is enabled

#### Scenario: Refresh stays visible

- **WHEN** the operator opens System → Log
- **THEN** Refresh is visible, left-aligned, and not stretched across the card

#### Scenario: No duplicate Log title

- **WHEN** the operator opens System → Log
- **THEN** the tab is labeled Log and the card does not show another Log heading

#### Scenario: Download exceeds view cap when ring is larger

- **WHEN** the host log ring holds more than 64 KiB and the operator activates Download full log
- **THEN** the downloaded file size matches the ring’s used length (greater than 64 KiB when the ring holds more)

#### Scenario: Download equals ring when small

- **WHEN** the ring holds less than 64 KiB and the operator activates Download full log
- **THEN** the downloaded file contains the full current ring text
