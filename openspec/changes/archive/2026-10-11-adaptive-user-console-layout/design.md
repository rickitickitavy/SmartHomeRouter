# Design

## Context

See proposal.md for motivation. Today:

- `data/user.html` shell (`.user-console-shell`) uses `max-width: 56rem` and large cm padding, so phones never get a true full-bleed console.
- View-mode grids (`renderConsoleGrid` in `user.html`, `placeRenderGrid` in `index.html`) set inline `gridTemplateColumns` from persisted `columns`, which overrides CSS.
- View spacers use `visibility: hidden`, so they still occupy grid cells and leave holes.
- Industry guidance (web.dev container queries; CSS unit best practice): size reusable dashboard widgets from their **container**, not the viewport; avoid true physical `cm` for screen layout (CSS `cm` maps to the reference pixel ≈ 37.8px/cm). Prefer `dvh`/`svh` over `vh` for mobile full-height shells.

## Goals / Non-Goals

**Goals:**

- Full-viewport simple-user shell; view-mode 1/2-column rules keyed off console content width at 13 cm / 10 cm (CSS reference).
- CSS-first layout (container queries + grid) so resize/orientation updates without JS column math.
- Same view-mode rules on Place preview as on `/user.html`.
- Editor dialog keeps saved `columns` for authorship and drag-reorder.

**Non-Goals:**

- Redesigning widget controls, themes, or console JSON schema.
- Changing operator gateway sidebar chrome beyond Place view-mode consoles.
- True physical centimetre accuracy on every device DPI (impossible with CSS alone).
- Auto-wrapping to 3+ columns on very wide screens in view mode (spec caps at two).

## Decisions

### 1. Breakpoints as CSS centimetres of the console container

Use `container-type: inline-size` on the view-mode console panel (or grid host) and `@container` queries at `13cm` / track max `10cm`.

- **Why:** Matches the product rule (“console width”) better than viewport media queries when Place sits inside the gateway main panel.
- **Alternative considered:** Viewport `@media (max-width: 13cm)` — rejected because Place width ≠ viewport.
- **Alternative considered:** Convert 13 cm → ~491px and use `px` only — acceptable fallback if a target browser mishandles `cm` in container queries; keep custom properties documenting the cm intent.

Equivalent reference mapping (for docs/tests): `1cm ≈ 37.795px` → `13cm ≈ 491px`, `10cm ≈ 378px`.

### 2. View-mode grid CSS, no inline column style

Stop setting `element.style.gridTemplateColumns` in `renderConsoleGrid` / `placeRenderGrid`. Use a distinct class (e.g. `console-grid console-grid-view`) with:

- Default (narrow): `grid-template-columns: minmax(0, 1fr);` (100% width stack).
- `@container (min-width: 13cm)`: `grid-template-columns: repeat(2, minmax(0, min(50%, 10cm)));` plus optional `justify-content: start` so surplus width does not stretch tracks past 10 cm.

Editor grid keeps the existing inline/repeat(`columns`) behavior and class without the view container rules.

### 3. Full viewport shell

For `#userShell` / `.user-console-shell`: remove `max-width: 56rem`; set width/height to `100%` of `html, body` sized with `100dvh` (fallback `100vh`); tighten padding on narrow widths; keep toolbar + scrollable `.user-console-panels`. Place view-mode host gets the same panel/grid classes so widget rules match; Place need not steal the whole gateway chrome, but its console content width drives the container query.

### 4. Hidden widgets must not take cells

In view mode, either omit spacer nodes from the DOM or style `.console-widget-spacer-view { display: none; }` (not `visibility: hidden`). Prefer `display: none` so existing append loops stay simple. Editor spacers remain visible for layout authorship.

### 5. Typography scale tied to the same surface

On the view-mode console, keep system UI font; set a slightly larger base for widget titles/values under the narrow container (e.g. widget title ~1.05–1.15rem, switches keep ≥44px touch targets per web-console skill). Do not invent a second visual language; stay on existing tokens (`--accent`, `--surface`, etc.).

### 6. Shared CSS, duplicated HTML helpers

Put shell/grid rules in `data/css/all.css` (already shared). Keep the small JS change mirrored in `user.html` and `index.html` (no new bundler). No firmware C++ changes beyond the propose-time `FIRMWARE_VERSION` bump already applied (`3.1.09`).

## Risks / Trade-offs

- [CSS `cm` ≠ physical ruler] → Document CSS-reference centimetres; validate on a phone and a tablet against the ~491px / ~378px equivalents.
- [Older WebViews without container queries] → Fallback: narrow single-column default; optional `@supports` or a single viewport media query mirroring 13 cm so phones still stack.
- [Two-column max-width leaves side margins on ultrawide] → Acceptable; matches “≤10 cm” and avoids unreadably wide widget rows.
- [Editor vs view diverge] → Intentional; call out in Consoles UI only if operators confuse Place preview with the edit dialog (Place follows view rules).

## Migration Plan

1. Ship UI assets with firmware OTA (console HTML/CSS in image).
2. No data migration: persisted `columns` still used by the editor; view mode ignores it for column count.
3. Rollback: previous firmware restores old capped shell and inline column grids.

## Open Questions

None that block implementation; physical validation of the 13 cm feel on target phones can tune padding only without changing the stated breakpoints.
