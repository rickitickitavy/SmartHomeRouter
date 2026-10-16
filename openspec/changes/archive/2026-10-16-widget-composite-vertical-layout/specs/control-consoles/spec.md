# Spec Delta: control-consoles

## MODIFIED Requirements

### Requirement: Consoles persist with widgets and devices
The host MUST persist control consoles. Each console MUST have a unique id, a name, an active flag, and an ordered list of widgets. Each widget MUST have a title, a show-group-on-off flag, a viewport (`vertical`, `grid`, or `composite`), optional grid Columns and Rows when viewport is `grid`, optional composite layout when viewport is `composite`, a `fullWidth` flag (default false), and an ordered list of bindings (for `vertical` and `grid`, or as device-cell content for `composite`). A binding MUST target either a registered device (optional channel, title, optional unit, optional param/custom cluster/attrId, optional view style and gauge config) or an automation by `name` (optional row title). Empty consoles MUST be allowed. After reboot the same consoles MUST load. The host MUST allow at most 32 consoles. Device-only exports from older firmware MUST still load. Missing viewport MUST load as `vertical`; missing view style as `default`; missing `fullWidth` as false. Viewport value `horizontal` on load MUST behave as `vertical`. Spacer widgets MUST NOT be retained after load or save.

#### Scenario: Save and reload with automation binding
- **WHEN** an editor saves console `Kitchen` with one widget that binds automation `yard-light`
- **THEN** after reboot `Kitchen` still has that widget and automation binding

#### Scenario: Thirty-third console rejected
- **WHEN** the host already has 32 consoles and an editor saves a new one
- **THEN** the host does not persist the thirty-third console and returns an error

#### Scenario: Save and reload grid viewport and gauge
- **WHEN** an editor saves a widget with viewport `grid`, Columns 2, Rows 2, and a gauge binding
- **THEN** after reboot that widget still has those viewport and gauge settings

#### Scenario: Legacy horizontal becomes vertical
- **WHEN** stored JSON has a widget with viewport `horizontal`
- **THEN** after load that widget behaves as viewport `vertical`

#### Scenario: Spacer widgets dropped
- **WHEN** stored JSON includes a spacer widget between two real widgets
- **THEN** after load and subsequent save only the two real widgets remain in order

#### Scenario: Full width persists
- **WHEN** an editor saves a widget with `fullWidth` true
- **THEN** after reboot that widget still has `fullWidth` true

### Requirement: Widget viewport layout
Each widget MUST have a viewport of `vertical` (default), `grid`, or `composite`. Missing viewport on load MUST behave as `vertical`. Viewport `horizontal` on load MUST behave as `vertical`. When viewport is `grid`, the widget MUST store positive integer Columns and Rows (defaults **2** and **2** when missing or invalid). In view mode, bindings MUST lay out according to viewport: vertical stacks top-to-bottom; grid fills cells left-to-right then top-to-bottom for Columns × Rows; composite lays out per the composite geometry requirement. If there are more bindings than Columns × Rows on a grid, remaining bindings MUST continue in additional rows using the same column count. Controls and gauges inside a grid cell, composite cell, or vertical row MUST scale to fit that slot. Binding view styles `default`, `gauge`, and `icon` MUST keep their existing behaviors inside those slots.

#### Scenario: Default viewport is vertical
- **WHEN** a saved widget has no viewport field
- **THEN** view mode stacks bindings vertically as today

#### Scenario: Grid fill order
- **WHEN** a widget viewport is `grid` with Columns 2 and Rows 2 and four numeric bindings
- **THEN** view mode shows them in a 2×2 grid filled left-to-right, top-to-bottom

#### Scenario: Grid overflow adds rows
- **WHEN** a widget viewport is `grid` with Columns 2 and Rows 2 and five bindings
- **THEN** the fifth binding appears on a third row in the first column

#### Scenario: Horizontal row
- **WHEN** stored JSON has a widget with viewport `horizontal` and three bindings
- **THEN** view mode stacks those three bindings vertically (legacy horizontal is treated as vertical)

### Requirement: Gauge appearance and scaling
In view mode, a gauge binding MUST render a Home Assistant–like arc or linear gauge using the configured min, max, value, and color ranges. The gauge MUST scale to fit its viewport cell (grid or composite) or vertical row without overflowing the widget content box. Values outside min/max MUST clamp to the scale ends for drawing; the numeric readout MAY still show the raw value.

#### Scenario: Arc fits grid cell
- **WHEN** a grid viewport cell shows an arc gauge
- **THEN** the gauge fits inside that cell without overlapping neighboring cells

#### Scenario: Linear fits horizontal slot
- **WHEN** a composite viewport cell shows a linear gauge binding
- **THEN** the bar scales to the cell width and stays within the cell

### Requirement: Binding icon scale multiplies viewport fit
When a binding uses view style `icon`, the binding MUST store an optional numeric `iconScale` (default `1` when missing or invalid). In view mode the rendered icon size MUST equal the size produced by the widget viewport fit scale (grid cell, composite cell, or vertical row) **multiplied by** `iconScale`. `iconScale` MUST NOT replace or disable the viewport fit scale. Values less than or equal to zero MUST behave as `1`.

#### Scenario: Default icon scale is one
- **WHEN** an icon binding is saved without `iconScale`
- **THEN** after reload the icon uses only the viewport fit size (effective multiplier `1`)

#### Scenario: Icon scale multiplies grid fit
- **WHEN** an icon binding in a grid cell has `iconScale` `1.5` and the cell fit would make the icon 20px tall
- **THEN** the icon renders at 30px tall (1.5 × fit), not 1.5× some fixed base that ignores the cell

#### Scenario: Icon scale multiplies composite fit
- **WHEN** an icon binding in a composite cell has `iconScale` `2` and the cell fit would make the icon 16px tall
- **THEN** the icon renders at 32px tall

### Requirement: Widgets stack in a reorderable grid
Widgets MUST sit in a single vertical stack in edit mode. The editor MUST reorder widgets only up and down (drag onto another stack position or up/down controls). Left/right placement MUST NOT be offered. Spacer/empty placeholder widgets MUST NOT appear. A widget-styled add control MUST sit after the last widget and MUST move down when a widget is added.

#### Scenario: Add widget
- **WHEN** the editor activates the add-widget control
- **THEN** a new widget appears and the add control sits after it

#### Scenario: Drag widget
- **WHEN** the editor drops widget B onto widget A’s place in the vertical stack
- **THEN** the saved order places B where A was and no spacer widgets are inserted

#### Scenario: No left or right move
- **WHEN** the editor uses widget move controls
- **THEN** only up and down are available

### Requirement: Wide console uses two capped columns
In view mode, when the visible console content width is 13 cm or greater, visible widgets with `fullWidth` false or missing MUST fill a two-column grid left-to-right then top-to-bottom. Each such widget MUST take half of the row width and MUST NOT exceed 10 cm width. Widgets with `fullWidth` true MUST span 100% of the console content width (a full row). Saved console `columns` MUST NOT force more than two columns in this view mode for non-full-width widgets. Hidden widgets MUST NOT take a cell.

#### Scenario: Two-column fill order
- **WHEN** view mode shows four visible widgets with `fullWidth` false on a console at least 13 cm wide
- **THEN** widget order is top-left, top-right, bottom-left, bottom-right

#### Scenario: Widget max width
- **WHEN** view mode shows non-full-width widgets on a console wider than 20 cm
- **THEN** each widget column is at most 10 cm wide

#### Scenario: Saved many columns still two in view
- **WHEN** a console is persisted with `columns` greater than 2 and a simple user opens it in view mode at least 13 cm wide
- **THEN** visible non-full-width widgets still lay out in at most two columns under the width rules above

#### Scenario: Full width spans content
- **WHEN** view mode shows a widget with `fullWidth` true on a console at least 13 cm wide
- **THEN** that widget’s width is 100% of the console content width

## ADDED Requirements

### Requirement: Composite viewport geometry
When viewport is `composite`, the widget MUST store ordered rows. Each row MUST store height (positive number, unit `cm` or `u`) and ordered cells. Each cell MUST store width (positive number, unit `cm` or `u`), optional contour (default off), and content `empty`, `text`, or `device`. Device cells MUST carry a normal binding; text cells MUST store display text.

#### Scenario: Save composite with mixed cells
- **WHEN** an editor saves a composite widget with one row of a 3 cm cell (text), a 1 u cell (device), and an empty cell with contour on
- **THEN** after reboot those rows, widths, units, content types, and contour flags are unchanged

#### Scenario: Missing composite defaults empty
- **WHEN** a widget has viewport `composite` but no rows
- **THEN** view mode shows an empty composite body until rows are added

### Requirement: Composite size calculation
For each composite row, `cm` cell widths MUST be fixed. Remaining widget width MUST split among `u` cells as `((Widget_width − Σ cm widths) / Σ u widths) × cell_u`. Row height in `cm` is fixed; row height in `u` MUST use that same unit size. Layout MUST NOT require equal columns.

#### Scenario: Mixed cm and unit widths
- **WHEN** widget content width is 10 cm, a row has cells 2 cm, 1 u, and 1 u
- **THEN** the first cell is 2 cm wide and each unit cell is 4 cm wide

#### Scenario: Unit row height follows width unit
- **WHEN** a row height is 1 u and that row’s computed unit size is 4 cm
- **THEN** the row height is 4 cm

### Requirement: Widget fullWidth attribute
Each real widget MUST store boolean `fullWidth` (default false). When false, view-mode width rules MUST match today’s adaptive layout. When true, width MUST be 100% of console content width. `fullWidth` MUST persist through save and reboot.

#### Scenario: Default not full width
- **WHEN** a widget is saved without `fullWidth`
- **THEN** after reload it behaves as `fullWidth` false

#### Scenario: Full width on narrow console
- **WHEN** view mode is under 13 cm wide and a widget has `fullWidth` true
- **THEN** that widget still uses 100% of the console content width
