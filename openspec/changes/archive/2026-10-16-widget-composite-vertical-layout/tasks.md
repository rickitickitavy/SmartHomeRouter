# Tasks

## 1. Host persist model

- [x] 1.1 Add `CONSOLE_VIEWPORT_COMPOSITE`, map legacy `horizontal` to vertical on parse, and stop emitting `horizontal` on serialize — verify: load JSON with `"viewport":"horizontal"` yields vertical; save writes `"vertical"` or omits horizontal
- [x] 1.2 Add `fullWidth` on `ConsoleWidget` (default false); parse/serialize round-trip — verify: save with `fullWidth:true`, reboot/reload JSON still true; missing field loads false
- [x] 1.3 Skip widget-level `spacer: true` on parse and never write spacer widgets — verify: mixed real+spacer list loads as real widgets only; save has no spacer entries
- [x] 1.4 Implement composite rows/cells parse, free, and JSON append with caps (rows/cells) and device-cell bindings reusing binding parse — verify: composite fixture round-trips text/device/empty, cm/u, contour; over-cap or OOM fails cleanly
- [x] 1.5 Bump `FIRMWARE_VERSION` build by 1 on apply (propose already set `3.4.26`) — verify: `include/Defines.h` reflects the implementation build

## 2. Console editor authoring (vertical stack)

- [x] 2.1 Force editor widget list to a single vertical stack; remove left/right move pad buttons and horizontal drop targeting — verify: only up/down (and stack drag) reorder; no left/right controls
- [x] 2.2 Remove spacer widget creation on reorder/drop; stop rendering spacer cards — verify: reorder two widgets leaves exactly two draft entries
- [x] 2.3 Stop using saved console `columns` for editor placement (ignore or hide) — verify: console previously saved with 3 columns still edits as one column stack
- [x] 2.4 Add Full width checkbox on each real widget card bound to draft `fullWidth` — verify: unchecked by default; check + Save persists true

## 3. Viewport UI: drop Horizontal, add Composite

- [x] 3.1 Viewport select options: Vertical, Grid, Composite only; keep Grid Columns/Rows behavior unchanged — verify: Horizontal absent; Grid still shows Columns/Rows and preview matches prior Grid
- [x] 3.2 Composite editor: Add row, left height configurer (cm/u), per-cell top width configurers, Add cell, content type text/device/empty, contour — verify: draft matches design JSON shape; Cancel discards unsaved rows
- [x] 3.3 Wire Composite preview sizing with the cm/u formula and optional cell contour CSS — verify: 10 cm widget with cells 2 cm + 1 u + 1 u yields 2 cm / 4 cm / 4 cm in preview
- [x] 3.4 Preserve Vertical binding add/edit and binding view styles (default/gauge/icon) inside Composite device cells — verify: gauge/icon device cell preview matches existing binding dialog behavior without sending commands

## 4. View mode (Place + simple-user)

- [x] 4.1 Render Composite viewport on Place and `user.html` with same size math and cell content as editor preview (live controls for device cells) — verify: Place and user page match for a saved composite console
- [x] 4.2 Honor `fullWidth` in wide layout (100% content width / full row) while `fullWidth` false keeps two-column ≤10 cm rules; Vertical and Grid unchanged — verify: mixed fullWidth and normal widgets on ≥13 cm console
- [x] 4.3 CSS in `all.css` for vertical editor stack, composite rows/cells, contour, and full-width class — verify: no horizontal editor layout; contours visible when enabled
- [x] 4.4 Integration smoke: Vertical, Grid, and Composite widgets with default/gauge/icon bindings; Save; reload; Place + user page — verify: Horizontal gone; spacers gone; Full width works; no device commands from editor preview
