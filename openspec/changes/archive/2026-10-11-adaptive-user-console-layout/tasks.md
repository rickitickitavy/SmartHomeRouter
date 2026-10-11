# Tasks

## 1. View-mode CSS shell and grid

- [x] 1.1 Update `.user-console-shell` (and shared view-mode panel host styles) in `data/css/all.css` so the simple-user shell is full viewport width/height (`100%` / `100dvh` with `100vh` fallback), remove the `56rem` max-width cap, and keep toolbar + scrollable panels; verify in a narrow browser window that the shell spans the viewport with no side max-width gutters.
- [x] 1.2 Add a view-mode grid class (e.g. `.console-grid-view`) with `container-type: inline-size`, default one full-width column, and `@container (min-width: 13cm)` two tracks `minmax(0, min(50%, 10cm))` left-to-right; verify resizing the container across ~13 cm switches between 1 and 2 columns and tracks never exceed 10 cm.
- [x] 1.3 Change `.console-widget-spacer-view` to `display: none` (or equivalent) so spacers do not occupy grid cells; verify a console with a spacer between two widgets shows those widgets adjacent with no empty hole in view mode.
- [x] 1.4 Tune view-mode widget title/value type and keep control min-height ≥ 44px using existing tokens; verify titles remain readable on a phone-width window and switches stay touch-sized.

## 2. User page and Place JS

- [x] 2.1 In `data/user.html` `renderConsoleGrid`, stop setting inline `gridTemplateColumns` from saved `columns`, apply the view-mode grid class, and leave spacers non-participating; verify a console saved with `columns: 4` still shows at most two columns (or one when narrow) on the user page.
- [x] 2.2 In `data/index.html` `placeRenderGrid`, mirror the same view-mode class and no inline columns; verify Place preview matches the user-page column rules at the same content width.
- [x] 2.3 Confirm the Consoles editor dialog still uses saved `columns` for its edit grid and drag-reorder; verify editing a 3-column console still shows three authoring columns in the dialog.

## 3. Integration check

- [x] 3.1 Manually check narrow (<13 cm console width) and wide (≥13 cm) cases on user page and Place: full-bleed shell on user page, 1-column vs 2-column capped widgets, hidden spacers ignored; verify no C++/API changes are required beyond the apply bump of `FIRMWARE_VERSION` to `3.1.11`.

## Workflow follow-up

- Archive the change after implementation review.
- Apply workflow will bump `FIRMWARE_VERSION` build again when source UI changes land.
