# Spec Delta: web-console

## MODIFIED Requirements

### Requirement: Place view-mode consoles match adaptive user layout
When the gateway console shows an assigned console in Place (or equivalent operator view mode), that view-mode surface MUST use the same full-viewport-friendly shell sizing and the same one- vs two-column widget width rules as the simple-user control page, including `fullWidth` spanning. The Consoles editor dialog MUST author widgets in a vertical stack only (not a multi-column authoring grid). Saved console `columns` MUST NOT drive editor placement.

#### Scenario: Place narrow width stacks widgets
- **WHEN** an operator opens Place view for a console whose content width is under 13 cm
- **THEN** visible widgets stack in one full-width column as on the simple-user page

#### Scenario: Editor keeps authoring columns
- **WHEN** an editor opens the Consoles edit dialog for a console saved with three columns
- **THEN** the edit surface shows a single vertical widget stack and does not use three-column placement or left/right targets

### Requirement: Console editor sets widget viewport
In the Consoles edit dialog, each widget MUST expose a Viewport control with options Vertical, Grid, and Composite (default Vertical). Horizontal MUST NOT be offered. When Grid is selected, the dialog MUST show Columns and Rows integer fields defaulting to 2 and 2. When Composite is selected, the dialog MUST show composite row/cell authoring (Add row, per-row height configurer on the left, per-cell width configurers stacked horizontally above cells). Changing viewport or layout size MUST update the in-dialog preview without persisting until Save. Cancel or reload without Save MUST discard unsaved viewport edits. Vertical and Grid preview and binding behaviors MUST match today’s Vertical and Grid.

#### Scenario: Grid fields appear
- **WHEN** the editor sets a widget Viewport to Grid
- **THEN** Columns and Rows fields are visible and default to 2 and 2 for a new grid selection without prior values

#### Scenario: Vertical hides grid fields
- **WHEN** the editor sets Viewport to Vertical
- **THEN** Columns and Rows fields are hidden and composite row editors are hidden

#### Scenario: Composite editor appears
- **WHEN** the editor sets Viewport to Composite
- **THEN** Add row and row/cell size configurers are visible and Columns/Rows are hidden

#### Scenario: No Horizontal option
- **WHEN** the editor opens the Viewport select
- **THEN** Horizontal is not listed

### Requirement: User and Place pages render viewport and gauges
The simple-user control page and Place view-mode MUST lay out each widget’s bindings according to that widget’s viewport (`vertical`, `grid`, or `composite`) and MUST render gauge-styled numeric bindings as scaled arc or linear gauges and icon-styled binary, On/Off, and automation bindings as Material Design Icons. Edit-mode preview in the Consoles dialog SHOULD reflect the same viewport, gauge, and icon choices using display-only controls (no device commands). Binding view styles MUST keep existing Default / Gauge / Icon behavior.

#### Scenario: User page grid gauges
- **WHEN** a simple user opens a console whose widget uses grid viewport and arc gauges
- **THEN** bindings appear in the grid and gauges fit their cells

#### Scenario: Place matches user viewport
- **WHEN** an operator opens the same console in Place view mode
- **THEN** viewport and gauge presentation match the simple-user page for that layout

#### Scenario: User page binary icon
- **WHEN** a simple user opens a console with a binaryInput binding using view style icon
- **THEN** the binding shows the chosen Material Design Icon instead of a default text/control-only plate

#### Scenario: User page On/Off icon
- **WHEN** a simple user opens a console with an On/Off binding using view style icon
- **THEN** the binding shows the icon control (not a switch)

#### Scenario: User page automation icon
- **WHEN** a simple user opens a console with an automation binding using view style icon
- **THEN** the binding shows the icon control for that automation

#### Scenario: User page composite layout
- **WHEN** a simple user opens a console whose widget uses composite viewport with mixed cm and unit cells
- **THEN** cells size per the composite size rules and device cells show their bindings

## ADDED Requirements

### Requirement: Console editor vertical-only widget reorder
In the Consoles edit dialog, widgets MUST stack vertically. Move controls MUST offer only up and down. Empty spacer widget cards MUST NOT be shown or created when reordering. Drag-and-drop MUST reorder within the stack without inserting spacer placeholders.

#### Scenario: Move pad has up and down only
- **WHEN** the editor shows a widget move pad
- **THEN** left and right move buttons are absent and up and down remain

#### Scenario: Reorder without spacers
- **WHEN** the editor moves the second widget above the first
- **THEN** the draft contains exactly those two widgets in the new order with no spacer entries

### Requirement: Console editor Full width checkbox
Each real widget in the Consoles edit dialog MUST show a Full width checkbox bound to that widget’s `fullWidth` flag (unchecked by default). Changing it MUST update the draft and preview width behavior without persisting until Save.

#### Scenario: Checkbox defaults unchecked
- **WHEN** the editor adds a new widget
- **THEN** Full width is unchecked

#### Scenario: Check full width in draft
- **WHEN** the editor checks Full width on a widget and saves
- **THEN** the persisted widget has `fullWidth` true

### Requirement: Console editor composite cell authoring
When Viewport is Composite, the editor MUST provide Add row. Each row MUST show a height configurer on the left (value plus cm or units). Each cell MUST show a width configurer above it (value plus cm or units), stacked horizontally across the row. The editor MUST let the operator set cell content to text, device (binding dialog), or empty, and MUST offer an optional cell contour control. Preview MUST reflect sizes and content without sending device commands.

#### Scenario: Add row
- **WHEN** the editor clicks Add row on a composite widget
- **THEN** a new row appears with at least one cell and left height configurer

#### Scenario: Configure cell width units
- **WHEN** the editor sets a cell width to 2 units
- **THEN** the draft stores width 2 with unit `u` and the preview updates

#### Scenario: Empty cell with contour
- **WHEN** the editor sets a cell to empty and enables contour
- **THEN** preview shows an outlined empty cell and save persists those settings
