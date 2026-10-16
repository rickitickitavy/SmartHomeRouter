# Tasks

## 1. Operator console recovery (`index.html`)

- [x] 1.1 Add a signed-in `recoverConsoleLiveLinks` helper that reopens wanted `/ws/devices` and `/ws/consoles` sockets (clear stale CLOSED/CLOSING handles and pending reconnect timers; no-op when already OPEN/CONNECTING), gated on session and `!firmwareRebootWaitActive` — verify by temporarily closing a socket in DevTools then calling the helper and seeing a new OPEN connection
- [x] 1.2 Extend that helper to re-arm wanted timers (`startSessionWatch` and active section polls: status, log, automation list, pairing/found when those flows are active) and trigger one immediate refresh for each active live view — verify Status (or another active poll) fetches again on recovery without a full reload
- [x] 1.3 Wire `document.visibilitychange` (when `visibilityState === "visible"`) and `window.pageshow` to the helper; ensure login overlay path is a no-op — verify switching away and back to a signed-in tab runs recovery; with login shown, no WS opens
- [x] 1.4 Soften `createLiveSocketLiveness` so while `document.hidden` it does not close the socket for pong timeout / still-awaiting-pong on the ping tick — verify backgrounding the tab for > pong window does not force-close an otherwise healthy socket before visibility recovery

## 2. User console recovery (`user.html`)

- [x] 2.1 Mirror the same recovery helper, visibility/pageshow wiring, and liveness hidden-tab guard for the sockets and timers `user.html` owns — verify backgrounding then returning to a signed-in user console reopens wanted live sockets and resumes session/live updates without reload

## 3. Integration check

- [ ] 3.1 Manually: sign in on operator console, open Devices, background the tab until the devices WS closes (or force-close it), return to the tab — verify the socket reopens and a device live event updates the table without reload
- [ ] 3.2 Manually: same return-to-tab path with Status (or Automation) active — verify poll timers keep updating; confirm login page still does not start live sockets on visibility
