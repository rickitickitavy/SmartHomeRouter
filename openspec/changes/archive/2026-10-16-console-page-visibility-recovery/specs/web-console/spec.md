# Spec Delta

## ADDED Requirements

### Requirement: Console recovers live links when the page becomes active

While the operator is signed in, when the console page becomes visible or shown again after being hidden or suspended, the console MUST ensure every live WebSocket that is supposed to be running is open or connecting, and MUST reopen any that are closed or otherwise not usable. After recovery, the signed-in UI MUST resume receiving router live events without requiring a full page reload.

#### Scenario: Devices live socket after tab returns

- **WHEN** the operator is signed in with devices live updates wanted, the tab was hidden long enough that the devices WebSocket closed, and the tab becomes visible again
- **THEN** the console reopens the devices live WebSocket and the Devices UI receives subsequent live updates from the router

#### Scenario: Consoles live socket after tab returns

- **WHEN** the operator is signed in with consoles live updates wanted, the consoles WebSocket is not open, and the page becomes active again
- **THEN** the console reopens the consoles live WebSocket and consoles/users/place live reloads resume

#### Scenario: Already healthy sockets stay up

- **WHEN** the page becomes active and every wanted live WebSocket is already open or connecting
- **THEN** the console does not tear those sockets down solely because the page became active

### Requirement: Console restarts wanted timers when the page becomes active

While the operator is signed in, when the console page becomes visible or shown again after being hidden or suspended, the console MUST ensure wanted interval timers (session watch and active section polls such as status, log, automation list, and pairing/found polls when those flows are active) are running again if they had stopped, and MUST trigger an immediate refresh for those active live views so the UI catches up after missed ticks.

#### Scenario: Status poll resumes after background

- **WHEN** the operator is signed in with Status polling wanted, the tab was backgrounded, and the page becomes visible again
- **THEN** Status polling is running and the Status panel refreshes from the router without a full page reload

#### Scenario: Session watch remains after activation

- **WHEN** the operator is signed in, session watch was wanted, and the page becomes active again
- **THEN** the session-alive watch timer is running

#### Scenario: Login page does not start live recovery

- **WHEN** the login form is shown (no signed-in session) and the page becomes visible again
- **THEN** the console does not open live WebSockets or start signed-in poll timers
