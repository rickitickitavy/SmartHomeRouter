# Design

## Context

See `proposal.md` for motivation. Observed client behavior in `data/index.html` / `data/user.html`:

- Live updates use `/ws/devices` and `/ws/consoles` with client ping every 15 s and a 5 s pong deadline (`createLiveSocketLiveness`). Reconnect is only scheduled from `onclose` via `setTimeout(..., 2000)`.
- Background tabs throttle timers; OS/browser may also close WebSockets when the page or device is inactive. A late ping + short pong window can close a still-viable socket, and a deferred reconnect may not run until the tab wakes — or may leave the UI without a scheduled reopen if state is inconsistent.
- Section polls (`status`, `log`, automation list, pairing/found) and `sessionWatchTimer` are plain `setInterval`s with no visibility recovery path today.
- Firmware WS endpoints and ping/pong JSON ops stay unchanged.

## Goals / Non-Goals

**Goals:**

- Single signed-in “page became active” recovery path that reopens wanted live sockets and re-arms wanted timers, then does a catch-up refresh for active views.
- Same pattern on operator (`index.html`) and user (`user.html`) consoles for the sockets/timers each page owns.
- Avoid duplicate sockets and avoid recovery while logged out or during firmware reboot wait.

**Non-Goals:**

- Changing host WebSocket protocol, idle-session rules, or MQTT/Zigbee event generation.
- Guaranteeing live delivery while the tab stays hidden (background delivery may still pause).
- Replacing all polls with WebSockets or introducing a service worker / SharedWorker.

## Decisions

### 1. Hook: `visibilitychange` + `pageshow`

Listen for `document.visibilitychange` when `document.visibilityState === "visible"`, and for `window.pageshow` (covers bfcache restore). Both call one `recoverConsoleLiveLinks()` (name flexible). Do not also force recovery on every `focus`/`blur` — that is noisier and overlaps visibility.

**Alternatives considered:** focus-only (misses some mobile cases; double-fires with clicks); polling `readyState` on a long interval while hidden (wastes work and still fights timer throttling).

### 2. Central recovery helper, reuse existing start/open functions

Recovery when signed in and `!firmwareRebootWaitActive`:

1. Clear in-flight live reconnect timers if present, then call `openDevicesLiveSocket` / `openConsolesLiveSocket` when the corresponding `*Wanted` flag is true (those already no-op when OPEN/CONNECTING).
2. If a wanted socket exists but `readyState` is `CLOSING` or `CLOSED`, null it out (and stop its liveness) then open again.
3. Re-enter existing start helpers for wanted polls/session watch (`startSessionWatch`, `startStatusPolling` / `startLogPolling` when those sections are active, automation/pairing/found starters when those flows are active) so missing intervals are recreated; helpers that early-return when a timer id is set stay safe.
4. Fire one immediate refresh for active live views (e.g. status/log/devices list/automations/consoles as already loaded by those start paths).

Keep logic shared in spirit across `index.html` and `user.html`; small duplication is acceptable if extraction would churn unrelated UI.

**Alternatives considered:** full `stop*` then `start*` on every visibility (extra reconnect churn); server-side longer idle timeout only (does not fix client timer stall).

### 3. Soften hidden-tab false death without redesigning keepalive

On recovery, reset liveness awaiting-pong state by rebinding or stopping+rebinding when reopening. Optionally, while `document.hidden`, skip closing the socket for pong timeout / “still awaiting pong” in the ping tick (treat as deferred check until visible). Prefer the visibility recovery as the hard requirement; the hidden-tab skip is a small defensive add if ping/pong keeps killing sockets before recovery runs.

**Alternatives considered:** much longer `LIVE_WS_PONG_MS` globally (slows real dead-socket detection while foregrounded).

### 4. Scope of “wanted” timers

Only restart timers the page already considers active (same conditions as today when entering a section or signing in). Do not invent new background polls. Login overlay → recovery is a no-op.

## Risks / Trade-offs

- [Burst of HTTP/WS on every tab return] → Mitigation: reuse open/start guards; one immediate refresh per active view, not a storm of all APIs.
- [Racing with intentional `stopDevicesLive` during reboot wait] → Mitigation: gate recovery on `!firmwareRebootWaitActive` and signed-in state, same as open helpers.
- [Mobile browsers fire visibility oddly] → Mitigation: idempotent helper; ignore when already healthy.

## Migration Plan

Ship with the LittleFS/web bundle that contains the updated HTML. No EEPROM or API migration. Rollback is reverting the two HTML files (and version bump).

## Open Questions

None that block implementation; if a particular poll (e.g. firmware reboot countdown) should also be nudged on visibility, implementers can include it only when that wait UI is active.
