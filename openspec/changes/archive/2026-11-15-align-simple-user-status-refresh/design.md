# Design

## Context

See proposal.md — Why. Operator Status (`data/index.html`) keeps a static Status DOM and calls `startStatusPolling` / `stopStatusPolling` from `showMainSection`, with `refreshGatewayStatus` → `applyGatewayStatus` every 2 s. Simple-user Status lives on the account tab (`data/user.html`). Today `refreshConsoleControls()` always calls `renderConsoleTabs()`, and when `showingAccountTab` is true that path runs `fillUserAccountPanel` (rebuilds Status DOM to "—", restarts polling). Devices-live upserts call that refresh often, so the Status card blinks. Host LED3 web-request holds (`WebConsole` request-hold pool) also fire more often during that churn.

## Goals / Non-Goals

**Goals:**
- Identical Status refresh algorithm: visibility-gated 2 s poll + in-place apply on both pages.
- Account/Status tab stable across console live refreshes.
- Keep Theme / Sign out / Status content requirements unchanged.

**Non-Goals:**
- Changing Status field inventory or single-column layout.
- Changing host LED3 semantics (still on for in-flight HTTP/WS); only reduce accidental churn.
- Sharing one JS file between pages (optional later); behavioral parity is enough.
- Changing operator Status section markup.

## Decisions

1. **Gate `renderConsoleTabs` rebuild when account/Status is open**  
   When `showingAccountTab` is true, console control refresh MUST NOT call the full tab/panel rebuild that recreates Status. Prefer updating console-only state, or no-op the heavy path until the user returns to a console tab.  
   Alternative: rebuild but preserve Status nodes — rejected as still fragile vs operator’s static DOM.

2. **Keep poll helpers aligned with operator**  
   Preserve `startStatusPolling` / `stopStatusPolling` / `refreshGatewayStatus` / `applyGatewayStatus` cadence (2 s) and visibility rules. Build Status DOM once when entering the account tab; later polls only `applyGatewayStatus`.

3. **Do not invent a new “request pool” client library**  
   Operator Status does not use a separate client pool; the “request pool” the operator observed is the host LED3 hold pool reacting to HTTP. Fix is stop needless Status rebuilds/extra polls, not a new frontend pool abstraction.

4. **Both pages stay in sync**  
   Any Status poll lifecycle tweak lands on both `index.html` and `user.html` if shared behavior changes; this change’s primary edit is `user.html`, with operator as the reference algorithm.

## Risks / Trade-offs

- **[Risk] Console live updates ignored while on account tab** → Mitigation: only skip Status-destroying rebuild; when user returns to a console tab, full `renderConsoleTabs` still runs and shows current device state.
- **[Risk] Accidental double timers** → Mitigation: keep existing `statusPollTimer != null` guard; start only on account enter, stop on leave/logout.
- **[Risk] Subtle divergence if operator later adds single-flight** → Mitigation: tasks require matching operator Status poll code path when changing shared algorithm.

## Migration Plan

- Frontend-only behavior fix embedded in host web assets; ship with host build bump.
- No settings/schema migration.

## Open Questions

- None blocking (align to operator algorithm as directed).
