# Design

## Context

See proposal.md. Today `#accountFullscreenBtn` sits in the Account theme row on `data/index.html` and is created next to theme Save on `data/user.html`. Toggle helpers (`toggleAccountFullscreen` / `syncAccountFullscreenButton`) already exist on both pages. Operator nav is `.sidebar-nav` (vertical desktop, horizontal strip ≤600px). Simple-user nav is `#consoleTabs.user-console-tabs`.

## Goals / Non-Goals

**Goals:**
- One always-visible Full screen control at the leading edge of main nav/tabs for operator and simple-user shells.
- Preserve toggle/fullscreen API behavior; remove Account-row duplicate.
- Tab names never covered by the button.

**Non-Goals:**
- Changing theme/Sign out placement.
- App-shell “kiosk mode” beyond the Fullscreen API.
- Slave or non-UI firmware behavior.

## Decisions

1. **Operator placement**  
   Insert a compact icon button as the first interactive control in `.sidebar-nav` (after or beside the brand title as needed so “left top” reads clearly). On the narrow horizontal strip, keep it as a fixed leading flex item that does not shrink; tabs remain in a scrollable sibling so labels stay readable.

2. **Simple-user placement**  
   Prepend the same control as the first child of `#consoleTabs` (or a thin wrapper: `[fullscreen | tabs…]`) so it stays left of console/account tabs when tabs are rendered.

3. **Reuse JS**  
   Keep existing fullscreen helpers and listeners; only change where the button is mounted and remove Account theme-row instances. Same `id` or shared class with one instance per page.

4. **CSS**  
   Shared rules in `data/css/all.css`: fixed-size icon button, `flex: 0 0 auto`, no absolute overlay on tab labels; tab strip `overflow-x: auto` for the tab group only.

5. **Hosts**  
   Shared `data/` assets cover P4 and S3; no board `#ifdef` in HTML.

## Risks / Trade-offs

- **[Risk] Narrow operator strip crowding** → Mitigation: non-shrinking button + scrollable tab buttons; do not use `position: absolute` over tabs.
- **[Risk] Simple-user tabs rebuilt in JS wipe the button** → Mitigation: prepend fullscreen each time tabs are rebuilt, or host it outside the cleared `innerHTML` container.
- **[Risk] Brand title vs “left top with tabs”** → Mitigation: on desktop sidebar, place control at the top of the nav strip so it reads as left-top chrome; on horizontal strip, leading slot before tabs.

## Migration Plan

- Ship with host UI **4.2.63**; users see the new location immediately after flash/data update.
- No settings migration.

## Open Questions

None blocking.
