# Tasks

## 1. Host: Reported data as typed automation params

- [x] 1.1 Add reportable param parse helpers and raise `AUTOMATION_PARAM_KEY_MAX` so `reportable:0xXXXX/0xXXXX` round-trips without truncation; verify a sample key stores and reloads intact
- [x] 1.2 Accept reportable params in typed listener and condition validation (`AutomationStore`); verify saving an automation with a Reported data condition and listener returns success (no longer the “need timer… complete condition” reject solely for that param)
- [x] 1.3 Resolve reportable live text in `copyEndpointStateForParam` (or equivalent) from cached reportable values; verify condition GREATER/EQUAL against a known converted reportable value evaluates as expected
- [x] 1.4 Match reportable listeners on status-path events for the bound cluster/attribute (honor `change` / `any`); verify a matching report triggers evaluation and a different reportable pair does not

## 2. UI: Optimistic actuator feedback (approach A)

- [x] 2.1 Add a per-device/channel pending override map (immediate commanded state, 5s timer, clear on confirm/contradict/timeout, replace on re-toggle) in `data/index.html` Place controls; verify switcher and icon flip immediately and revert after 5s without a status
- [x] 2.2 Wire group On/Off so each commanded On/Off member has its own pending entry; verify mixed confirm/timeout outcomes leave confirmed members alone
- [x] 2.3 Mirror the same pending map and wiring in `data/user.html`; verify Place and simple-user behave the same for single toggle and group On/Off
- [x] 2.4 Skip optimistic flip when the binding uses a custom click action (not a normal actuator command); verify click-action icons do not fake device state

## 3. Integration checks

- [x] 3.1 Save and reload an automation that listens and conditions on Reported data; trigger it with a real report and confirm actions run when the comparison is true (Debug log optional aid)
- [x] 3.2 Confirm Sensor value / CUSTOM / battery listen and condition paths still save and evaluate unchanged
