# Design

## Context

See proposal.md. Today the simple-user page creates a Full screen control next to theme Save on the Account tab in `data/user.html`. Toggle helpers (`toggleAccountFullscreen` / `syncAccountFullscreenButton` or equivalents) already exist there. Simple-user nav is `#consoleTabs.user-console-tabs`. Operator `data/index.html` keeps its current Account-tab Full screen control — **not** changed by this work.

## Goals / Non-Goals

**Goals:**
- One always-visible Full screen control at the leading edge of the simple-user tab strip.
- Preserve toggle/fullscreen API behavior on `user.html`; remove Account-row duplicate on that page only.
- Tab names never covered by the button.

**Non-Goals:**
- Any Full screen relocation or removal on the operator gateway console (`data/index.html`).
- Changing theme/Sign out placement beyond removing the simple-user Account Full screen duplicate.
- App-shell “kiosk mode” beyond the Fullscreen API.
- Slave or non-UI firmware behavior.

## Decisions

1. **Simple-user placement**  
   Prepend the control as the first child of `#consoleTabs` (or a thin wrapper: `[fullscreen | tabs…]`) so it stays left of console/account tabs when tabs are rendered.

2. **Operator**  
   Leave `data/index.html` Full screen where it is today (Account theme row). No leading `.sidebar-nav` Full screen from this change.

3. **Reuse JS**  
   Keep existing simple-user fullscreen helpers and listeners; only change where the button is mounted on `user.html` and remove the Account theme-row instance there. Same `id` or shared class with one instance on the simple-user page.

4. **CSS**  
   Rules in `data/css/all.css` scoped to simple-user tab chrome as needed: fixed-size icon button, `flex: 0 0 auto`, no absolute overlay on tab labels; tab strip `overflow-x: auto` for the tab group only. Avoid operator-sidebar layout changes unless required for a shared class already used by `user.html`.

5. **Hosts**  
   Shared `data/user.html` covers P4 and S3; no board `#ifdef` in HTML.

## Risks / Trade-offs

- **[Risk] Simple-user tabs rebuilt in JS wipe the button** → Mitigation: prepend fullscreen each time tabs are rebuilt, or host it outside the cleared `innerHTML` container.
- **[Risk] Narrow strip crowding** → Mitigation: non-shrinking button + scrollable tab buttons; do not use `position: absolute` over tabs.

## Migration Plan

- Ship with host UI build bump on apply; simple users see the new location immediately after flash/data update.
- Operators see no Full screen chrome change.
- No settings migration.

## Open Questions

None blocking.
