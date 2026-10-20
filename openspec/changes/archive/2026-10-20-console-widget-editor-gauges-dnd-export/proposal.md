# Proposal

## Why

Console operators editing composite widgets and gauges hit several small friction points: gauge color ranges are add/remove-only, cells cannot be dragged between rows, color picking has no useful presets, arc digit size does not stay proportional across cell sizes (and Place vs simple-user views can diverge), centered icons can look right-biased, and console export filenames collapse non-Latin names to underscores. These fixes make authoring and viewing match operator expectation without changing console persistence shape.

## What Changes

- Gauge color-range editor: allow editing an existing range’s **top value** and **color** in place (not only Add / Remove).
- Composite widget editor: **drag-and-drop cells**, including across rows; after a move, **recalculate row heights** with the existing composite unit/cm layout rules (and drop empty source rows).
- Color picker used for gauge ranges: add **eight preset swatches** — white, gray, light gray, red, green, light blue, yellow, orange.
- Arc gauge **digit font** scales proportionally with the rendered gauge size (option **B**: proportional with a small min/max clamp so tiny phone cells stay readable); **Place (operator) and simple-user (`user.html`) views MUST look identical** for the same layout.
- Icon bindings with horizontal align **center** MUST appear visually centered in the cell (fix for composite cells such as `mdi:engine` on the Котел console).
- Console **Export JSON** download filename MUST use the console name, with **transliteration** when the name is not Latin (instead of stripping Cyrillic to `_`).
- Numeric (“digital”) device params: checkbox **as logical** (label to the right). When checked, show compare threshold (**value &lt; threshold → FALSE**, else **TRUE**); View style is **Default|Icon only** (variant B, Gauge hidden); icons use bool yellow/muted styling (display-only). Persist `asLogical` + `logicalThreshold`.

## Capabilities

### New Capabilities

_(none)_

### Modified Capabilities

- `control-consoles`: proportional arc gauge digit scaling; icon center; Place/simple-user parity; numeric **as-logical** conversion + Icon when enabled.
- `web-console`: gauge range edit; composite DnD; color presets; digit scaling; centered icons; export translit; binding dialog **as logical** + threshold; Place/user render converted icons.

## Impact

- UI: `data/index.html` (Consoles editor, Place, export), `data/user.html` (simple-user view), `data/css/all.css` (presets, icon/gauge layout).
- Host: `FIRMWARE_VERSION` build bump only for this propose (`3.5.31` → `3.5.32`); no ConsoleStore JSON schema change expected (ranges already store `top` + `color`).
- No SPI / fragile OTA zones.
- `FIRMWARE_VERSION` build bumped to `3.5.32` at propose time.

- Additional scope (same change): numeric as-logical; propose bump continued to `3.5.39`.
