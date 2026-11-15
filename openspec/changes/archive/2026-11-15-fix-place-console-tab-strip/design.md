# Design

## Context

See proposal.md — Why. Place uses `#placeConsoleTabs` with classes `tabs user-console-tabs` inside `.place-console-shell` (`data/index.html`). Simple-user tabs under `.user-console-toolbar .user-console-tabs .tab-btn` set `white-space: nowrap`. Place-specific rules set `max-height: 44px` on `.tab-btn` but do not force nowrap on Place tabs, so long titles wrap; wrapped text + `overflow-x: auto` on `.user-console-tabs` produces both H and V scrollbars on PC (as in the admin screenshot).

## Goals / Non-Goals

**Goals:**
- Single-line Place tab labels.
- No visible H/V scrollbars on the Place tab strip on PC/desktop.
- Visual parity with simple-user tab row style, without adding Full screen to Place.

**Non-Goals:**
- Removing or relocating simple-user Full screen.
- Changing operator Account Full screen.
- Redesigning console panel content.
- Forcing the same overflow policy on every nested System/Network folder tab strip.

## Decisions

1. **CSS-first fix scoped to Place**  
   Ensure Place tab buttons use `white-space: nowrap`, single-row flex (`flex-wrap: nowrap`), and strip overflow that does not paint scrollbars on desktop (e.g. `overflow-x: hidden` / `overflow-y: hidden` on the Place tab host, or equivalent that removes both scrollbars). Prefer scoping under `.place-console-shell` so simple-user toolbar behavior stays intact unless shared rules are the true cause.

2. **Match simple-user label treatment, not chrome**  
   Copy the single-line tab label behavior from `.user-console-toolbar .user-console-tabs .tab-btn`. Do **not** introduce `.user-console-tab-chrome` + Full screen into Place.

3. **Overflow when many tabs**  
   With scrollbars forbidden on PC, excess tabs may clip or the strip may rely on layout fitting without scrollbar UI. Accept clipping/ellipsis only if needed to satisfy no-scrollbar + single-line; do not reintroduce dual scrollbars. (If product later wants swipe-without-scrollbar-chrome, that can be a follow-up.)

4. **Operator + simple-user sync**  
   Shared `.tab-btn` / `.user-console-tabs` edits MUST be checked against `data/user.html` so simple-user Full screen + nowrap still work. Place-only selectors preferred when behavior must diverge (no Place Full screen; no PC Place scrollbars).

## Risks / Trade-offs

- **[Risk] Many long names clip on narrow desktop panes** → Mitigation: nowrap + no scrollbar is the requested PC look; verify with the reported admin screenshot case first.
- **[Risk] Changing shared `.user-console-tabs { overflow-x: auto }` breaks simple-user narrow overflow** → Mitigation: override under `.place-console-shell` instead of deleting simple-user overflow.
- **[Risk] max-height 44px alone hides wrapped text badly** → Mitigation: nowrap removes wrap so max-height stays a single-line row.

## Migration Plan

- CSS/markup only in host web assets; host build bump.
- No settings migration.

## Open Questions

- None blocking (PC Place: single-line, no scrollbars, no Full screen).
