# Proposal: console widget viewport and gauges

## Why

Console widgets only stack bindings vertically and show numeric values as plain text. Operators need denser layouts (grid / horizontal) and Home Assistant–style gauges so temperature, humidity, and other numeric parameters are readable at a glance on phones and wall panels.

## What Changes

- Each widget gains a **viewport** mode: `vertical` (default, current behavior), `horizontal`, or `grid`.
- When viewport is `grid`, the editor shows **Columns** and **Rows** (default **2 × 2**). Bindings fill left-to-right, top-to-bottom; gauges and controls scale to fit each cell (or horizontal slot).
- When adding or editing a **device** binding on a widget, the editor offers the same **device / channel / param** (and custom cluster + attrId) fields used by automation “device to trigger”, with the same dependent-select behavior.
- For numeric device parameters and custom numeric attrs, the binding MAY choose **view style**: `default` (current text/control) or `gauge`.
- When view style is `gauge`, a **Gauge** config group appears: type (`arc` default, or `linear`), min value, max value, and an ordered list of color ranges (each range has a top limit and color; bottom is the previous top, first bottom is gauge min). Ranges are edited via Top + color picker fields below the list.
- Gauge rendering follows a Home Assistant–like look (arc / linear bar with segmented colors) and scales to fit the viewport cell or row.
- Existing consoles without the new fields keep vertical viewport and default view style (backward compatible; no **BREAKING** API for simple users).

## Capabilities

### New Capabilities

_(none)_

### Modified Capabilities

- `control-consoles`: persist widget viewport (and grid columns/rows); persist binding param selection (like automation listen), view style, and gauge config; view-mode layout and gauge rendering rules.
- `web-console`: console editor UI for viewport, binding trigger-like fields, view style, and gauge config; user/Place view rendering for horizontal/grid and gauges.

## Impact

- Host: `ConsoleStore` / `ConsoleWidget` / `ConsoleBinding` JSON parse, persist, export/import round-trip for new fields.
- UI: `data/index.html` console editor + Place preview; `data/user.html` view-mode layout and gauge drawing; `data/css/all.css` for viewport grids and gauge sizing.
- No SPI / fragile firmware-update zones.
- `FIRMWARE_VERSION` build bumped to `3.2.10` at propose time.
