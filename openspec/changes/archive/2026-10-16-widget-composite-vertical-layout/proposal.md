# Proposal: widget composite vertical layout

## Why

Console design-time placement still uses a multi-column grid with empty spacer widgets and left/right moves, while widget viewports include Horizontal next to Vertical and Grid. Operators need a simpler vertical-only widget stack, a flexible Composite viewport for irregular cell layouts, and an optional full-width widget flag—without changing how Vertical, Grid, or binding view styles (default / gauge / icon) behave.

## What Changes

- **BREAKING (design-time only):** Console edit dialog places widgets in a **single vertical stack**. Left/right move controls and horizontal slot targeting are removed; only up/down reorder remains (plus existing drag reorder along the stack).
- **BREAKING (design-time only):** **Empty spacer widgets** are no longer created or required for placement. The editor does not insert spacer placeholders when reordering. Existing spacer widgets are dropped on load/save so they no longer reserve slots.
- **BREAKING:** Widget viewport option **Horizontal** is removed from the editor and from new saves. Saved widgets with viewport `horizontal` load as `vertical` (bindings still render with Vertical behavior).
- Widget viewport gains **Composite** alongside preserved **Vertical** and **Grid**. Binding view styles (`default`, `gauge`, `icon`) and their existing behaviors are unchanged.
- **Composite** viewport: authoring UI offers **Add row**; each row has a left-side height configurer (centimeters or units); each cell has a top width configurer (centimeters or units). Fixed cm widths/heights stay absolute; unit sizes share the remaining widget width (or height derived from that unit scale) using  
  `((Widget_width − Σ cm widths) / Σ unit widths) × cell_unit_width`  
  (and the analogous rule for row height vs cell-derived unit scale). Each cell MAY be text, a device binding, or empty; cell contour MAY be on or off.
- Each widget gains boolean **`fullWidth`** (default false / unchecked). When false, view-mode width rules stay as today; when true, the widget stretches to **100%** of the console content viewport width. Design-time exposes a Full width checkbox on each widget.

## Capabilities

### New Capabilities

_(none)_

### Modified Capabilities

- `control-consoles`: widget viewport set is `vertical` | `grid` | `composite` (no `horizontal`); persist composite row/cell geometry and cell content; persist `fullWidth`; spacer widgets no longer part of the active model; Vertical and Grid layout rules preserved.
- `web-console`: console editor vertical-only widget stack (no spacers, no left/right); Viewport UI drops Horizontal and adds Composite with row/cell configurers; Full width checkbox; Place / simple-user / editor preview render Composite and honor `fullWidth` while keeping Vertical and Grid (and binding view styles) as today.

## Impact

- Host: `ConsoleStore` / `ConsoleWidget` JSON parse, persist, export/import for viewport `composite`, composite layout payload, and `fullWidth`; drop or coerce `horizontal` and spacer widgets; `Defines.h` viewport constants.
- UI: `data/index.html` console editor + Place; `data/user.html` view mode; `data/css/all.css` for stack, composite, and full-width.
- No SPI / fragile firmware-update zones.
- `FIRMWARE_VERSION` build bumped to `3.4.26` at propose time.
