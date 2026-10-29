# console-connection-overlay Specification

## Purpose

Shows signed-in operators and simple users when the browser has lost the Mini-HA router HTTP or live WebSocket link, and when it is reconnecting, so stale live UI is not mistaken for a healthy connection.

## Requirements

### Requirement: Disconnected modal when link is down
While the user is signed in, when the main HTTP reachability to the router fails or a required live WebSocket closes and is not open, the page MUST show a blocking modal titled **Disconnected** with a disconnected picture or icon. The modal MUST appear on both the operator console page and the simple-user control page.

#### Scenario: WebSocket closes while signed in
- **WHEN** a signed-in operator or simple user has a required devices or consoles live WebSocket and that socket closes
- **THEN** the page shows the Disconnected modal with the disconnected picture or icon

#### Scenario: HTTP reachability fails while signed in
- **WHEN** a signed-in session’s main HTTP check to the router fails (network error / unreachable)
- **THEN** the page shows the Disconnected modal with the disconnected picture or icon

### Requirement: Reconnecting state on the same modal
While the signed-in page is actively trying to restore the lost HTTP or WebSocket link, the same modal MUST remain visible, MUST use a **RECONNECTING** picture or icon, and MUST show text that the UI is reconnecting to the router.

#### Scenario: Auto-reconnect after socket close
- **WHEN** a required live WebSocket has closed and the page schedules or runs reconnect
- **THEN** the modal shows the RECONNECTING picture or icon and reconnecting-to-router text

### Requirement: Modal clears when connectivity returns
When the signed-in page again has working main HTTP reachability and every required live WebSocket is open (or no live socket is required in that moment), the Disconnected / Reconnecting modal MUST hide without a full page reload.

#### Scenario: Socket reopens
- **WHEN** a previously closed required live WebSocket opens again and HTTP reachability is OK
- **THEN** the connection modal is hidden and live UI updates resume

### Requirement: Sign-out and firmware reboot wait are not this modal
Intentional sign-out MUST NOT show the Disconnected / Reconnecting modal. The existing firmware-reboot wait UI MUST remain the feedback during a joined firmware reboot wait; that wait MUST NOT be replaced by this connection modal.

#### Scenario: User signs out
- **WHEN** a signed-in user signs out successfully
- **THEN** the connection modal is not shown as the login form appears

#### Scenario: Firmware reboot wait active
- **WHEN** the operator console is in firmware reboot wait after an update apply
- **THEN** the firmware reboot wait UI is shown and the connection modal is not used for that wait
