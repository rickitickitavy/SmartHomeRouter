# Spec Delta

## ADDED Requirements

### Requirement: Simple-user Full screen control lives in main tab chrome
After login, the **simple-user** control page SHALL show a **Full screen** control in the main tab strip chrome at the leading (left) end of the same row that holds the console/account tabs. The control SHALL remain visible while any console or Account tab is selected, including when the Account / user-name tab is not selected. The simple-user Account / user-name panel SHALL NOT be the only place that offers Full screen, and SHALL NOT keep a second Full screen button in the theme row. The **operator** gateway console NEED NOT change Full screen placement for this requirement.

#### Scenario: Simple user always sees Full screen
- **WHEN** a simple user is on the control-console page with console tabs visible
- **THEN** the Full screen control is visible at the leading end of the tab strip without opening the Account tab

#### Scenario: Single control on simple-user page
- **WHEN** a simple user opens the Account / user-name tab
- **THEN** theme and Sign out remain, and there is no second Full screen button in that theme row

#### Scenario: Operator Full screen unchanged by this requirement
- **WHEN** an operator uses the gateway console
- **THEN** Full screen placement remains as before this change (Account theme row), not relocated by this requirement

### Requirement: Simple-user Full screen control does not hide tab names
On the simple-user control page, the Full screen control SHALL occupy its own non-overlapping slot in the tab strip so console or account tab labels remain fully readable. It MUST NOT paint over, clip, or replace visible tab names.

#### Scenario: Tab labels remain readable
- **WHEN** the simple-user tab strip is shown with the Full screen control present
- **THEN** each visible tab’s name text is fully readable and not covered by the Full screen control

#### Scenario: Narrow strip scrolls tabs not under the button
- **WHEN** the viewport is narrow and simple-user tab labels require horizontal overflow
- **THEN** tab labels can still be read (via scroll or wrap of the tab area) without the Full screen control covering those names
