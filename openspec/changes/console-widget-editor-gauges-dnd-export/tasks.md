# Tasks

## 1. Gauge range editor and color presets

- [x] 1.1 Make each gauge color-range row editable for top value and color (keep Remove; sort by top on save) in `data/index.html`
- [x] 1.2 Add the eight preset swatches (white, gray, light gray, red, green, light blue, yellow, orange) next to the gauge range color control and wire them to the active color
- [x] 1.3 Style presets in `data/css/all.css` so they are clickable and distinct without looking disabled

## 2. Composite cell drag-and-drop

- [x] 2.1 Add drag-and-drop for composite cells within a row and across rows in the Consoles editor
- [x] 2.2 After drop, recalculate composite layout sizes with existing cm/`u` rules and remove emptied source rows
- [x] 2.3 Ensure nested binding clicks still open the binding dialog (drag from cell chrome / handle, not from inputs)

## 3. Arc digit proportional scaling (option B)

- [x] 3.1 Tune shared proportional font formula with min/max clamp in `applyCompositeContentFit` (and any related arc paths) so small top-row gauges are no longer disproportionately tiny vs large bottom-row gauges
- [x] 3.2 Keep Place (`data/index.html`) and simple-user (`data/user.html`) formulas identical so the same layout yields the same digit size
- [x] 3.3 Spot-check against Котел desktop and phone screenshots (top rows readable; bottom row still proportional)

## 4. Centered icon alignment

- [x] 4.1 Fix composite/icon CSS or layout so `alignH` center (cell or binding) visually centers icons such as `mdi:engine` with no right bias on Place and `user.html`
- [x] 4.2 Verify with the Котел console export (`mdi:engine`, showTitle false, center align)

## 5. Console export filename translit

- [x] 5.1 Change `exportConsoleJson` to build the download name from the console name with Cyrillic→Latin transliteration and safe-character sanitization
- [x] 5.2 Confirm Latin names stay readable (e.g. `Boiler.json`) and `Котел` becomes a Latin slug like `Kotel.json`

## 6. Version and verify

- [x] 6.1 Bump `FIRMWARE_VERSION` build by 1 when applying source changes (propose already set `3.5.32`)
- [x] 6.2 Manually verify all six behaviors in Consoles editor, Place, and simple-user view
