# Spec Delta

## ADDED Requirements

### Requirement: Simple-user Status refresh matches operator Status
While a simple user’s account/Status tab is visible, Status SHALL refresh with the same algorithm as the operator Status section: periodic `GET /api/status` on the same cadence used by the operator Status poll, applying payloads into the existing Status value nodes in place. Status polling SHALL run only while that Status surface is visible and SHALL stop when the user leaves it (for example switches to a console tab), matching operator start/stop when leaving the Status section.

#### Scenario: Same cadence while Status visible
- **WHEN** a simple user keeps the account/Status tab open and an operator keeps the Status section open
- **THEN** both pages issue Status polls on the same interval and update displayed Status values without a full page reload

#### Scenario: Polling stops off Status
- **WHEN** a simple user leaves the account/Status tab for a console tab
- **THEN** Status `/api/status` polling stops until that Status tab is opened again

### Requirement: Simple-user Status card is not rebuilt on console live refresh
While the simple-user account/Status tab is visible, console control refreshes driven by device/automation live updates SHALL NOT destroy and recreate the Status card DOM. Status values SHALL continue to update only through the in-place Status apply path (as on the operator Status section).

#### Scenario: Device live update does not flash Status
- **WHEN** the account/Status tab is open and a devices-live update would refresh console controls
- **THEN** Status row values do not reset to placeholders solely because of that console refresh

## MODIFIED Requirements

### Requirement: Simple user account tab shows Status
A simple user’s control-console page SHALL show the live Status summary on the account tab named by the signed-in user name. Status SHALL appear after Theme and Sign out on that tab. Status widgets on that page SHALL use a single column. Status SHALL obtain values from the gateway status API using the same refresh algorithm as the operator Status section and SHALL NOT contain settings forms that write configuration.

#### Scenario: Simple user opens account tab
- **WHEN** a simple user opens the tab named by their user name
- **THEN** the tab shows Theme, Sign out, and the Status groups in one column below them

#### Scenario: Status updates without card rebuild blink
- **WHEN** the account/Status tab stays open across successive Status poll responses
- **THEN** Status group structure remains and values update in place without recreating the Status card each poll
