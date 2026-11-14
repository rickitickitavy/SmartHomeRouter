# Spec Delta

## ADDED Requirements

### Requirement: Full screen control lives in main nav chrome
After login, both the operator gateway console and the simple-user control page SHALL show a **Full screen** control in the main navigation chrome at the leading (left) end of the same row or strip that holds the main section/console tabs. The control SHALL remain visible while any main section or console tab is selected, including when the Account / user-name tab is not selected. The Account / user-name panel SHALL NOT be the only place that offers Full screen, and SHALL NOT keep a second Full screen button in the theme row.

#### Scenario: Operator always sees Full screen
- **WHEN** an operator is signed into the gateway console and opens Status (or any other main section)
- **THEN** the Full screen control is visible in the main nav chrome without opening the Account tab

#### Scenario: Simple user always sees Full screen
- **WHEN** a simple user is on the control-console page with console tabs visible
- **THEN** the Full screen control is visible at the leading end of the tab strip without opening the Account tab

#### Scenario: Single control
- **WHEN** an operator or simple user opens the Account / user-name tab
- **THEN** theme and Sign out remain, and there is no second Full screen button in that theme row

### Requirement: Full screen control does not hide tab names
The Full screen control SHALL occupy its own non-overlapping slot in the nav/tab strip so main section or console tab labels remain fully readable. It MUST NOT paint over, clip, or replace visible tab names.

#### Scenario: Tab labels remain readable
- **WHEN** the main nav/tab strip is shown with the Full screen control present
- **THEN** each visible tab’s name text is fully readable and not covered by the Full screen control

#### Scenario: Narrow strip scrolls tabs not under the button
- **WHEN** the viewport is narrow and tab labels require horizontal overflow
- **THEN** tab labels can still be read (via scroll or wrap of the tab area) without the Full screen control covering those names
