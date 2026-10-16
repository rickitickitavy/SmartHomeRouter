# Proposal

## Why

When the browser tab with the web console is backgrounded (or the device/browser suspends timers), live WebSockets close and interval-driven UI updates stall. After the operator returns to the page, the console often stays stale and no longer receives router events until a full reload.

## What Changes

- On page activation (tab visible / page shown again), the signed-in console MUST re-check live WebSocket health and reopen any wanted socket that is not open or connecting.
- On the same activation, the console MUST ensure wanted polling/watch timers are still running (or restart them) and trigger an immediate refresh for active live views so missed updates catch up.
- Live-socket liveness (ping/pong) MUST NOT treat browser timer throttling while the page is hidden as a fatal dead socket when avoidable; recovery on visibility is the primary fix for stale UI after backgrounding.
- Apply the same recovery path on both the operator console (`index.html`) and the user console (`user.html`) where those sockets/timers exist.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: Require recovery of live WebSockets and wanted timers/polls when the console page becomes active again after being hidden or suspended, so the UI resumes receiving router events without a full reload.

## Impact

- Client JS in `data/index.html` and `data/user.html` (Page Visibility / pageshow handlers, live socket open/reconnect helpers, poll/session timer start helpers).
- No firmware HTTP/WebSocket protocol change required; existing `/ws/devices` and `/ws/consoles` endpoints and ping/pong ops remain.
- Firmware build bump already applied for this propose: `FIRMWARE_VERSION` `3.4.46` → `3.4.47`.
