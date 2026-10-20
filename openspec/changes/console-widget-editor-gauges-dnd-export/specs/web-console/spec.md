# Spec Delta

## MODIFIED Requirements

### Requirement: Console device dialog gauge config
When the selected binding parameter is numeric (including Measurement-class `sensorValue`, battery, position, light level, or custom) and the device is not a binary sensor type, the binding dialog MUST show a View style control with Default and Gauge. When Gauge is selected, the dialog MUST show a Gauge group: type Arc (default) or Linear, Min value, Max value, and a list of color ranges. Each list row MUST let the editor change that range’s top limit and color in place (not only Add / Remove). The dialog MUST provide Top and color-picker fields below the list to append a range. Bottom of each range is derived (first from Min, later from previous top). View style Gauge MUST be hidden or ignored for On/Off status, covering move status, automation bindings, and binary sensor types.

#### Scenario: Temperature offers Gauge
- **WHEN** an editor opens a device binding for a temperature Measurement device and the param is Sensor value
- **THEN** the dialog shows View style with Default and Gauge

#### Scenario: On/Off hides Gauge
- **WHEN** an editor selects an On/Off device with param Status
- **THEN** View style Gauge is hidden

#### Scenario: Gauge group appears
- **WHEN** the editor chooses View style Gauge for a temperature binding
- **THEN** type, min, max, and the color-range list editor are visible

#### Scenario: Add color range
- **WHEN** the editor enters a Top and color and adds a range
- **THEN** that range appears in the list and is included when the binding is saved

#### Scenario: Edit existing range top and color
- **WHEN** the editor changes an existing range’s top value and color in the list and saves the binding
- **THEN** the persisted gauge ranges use the edited top and color

### Requirement: Console editor composite cell authoring
When Viewport is Composite, the editor MUST provide Add row. Each row MUST show a height configurer on the left (value plus cm or units). Each cell MUST show a width configurer above it (value plus cm or units), stacked horizontally across the row. The editor MUST let the operator set cell content to text, device (binding dialog), or empty, and MUST offer an optional cell contour control. The editor MUST support drag-and-drop of cells within a row and across rows of the same widget; after a drop it MUST recalculate composite row heights with the existing size rules and MUST remove an emptied source row. Preview MUST reflect sizes and content without sending device commands.

#### Scenario: Add row
- **WHEN** the editor clicks Add row on a composite widget
- **THEN** a new row appears with at least one cell and left height configurer

#### Scenario: Configure cell width units
- **WHEN** the editor sets a cell width to 2 units
- **THEN** the draft stores width 2 with unit `u` and the preview updates

#### Scenario: Empty cell with contour
- **WHEN** the editor sets a cell to empty and enables contour
- **THEN** preview shows an outlined empty cell and save persists those settings

#### Scenario: Drag cell across rows
- **WHEN** the editor drags a cell from one composite row onto another row and drops it
- **THEN** the draft moves that cell, refreshes row heights, and removes the source row if it has no cells left

### Requirement: Place and user gauges show unitless value text
On the simple-user control page and Place view-mode, gauge-styled bindings MUST show the numeric value without a measure unit suffix. Arc gauges MUST use the proportional center value font sizing from the control-consoles arc font requirement (including shared min/max clamps), and Place MUST match the simple-user page for the same layout and viewport size.

#### Scenario: User page temperature gauge text
- **WHEN** a simple user opens a console with a temperature arc gauge
- **THEN** the center value shows digits only (no °C) at the proportional arc font size for that cell

#### Scenario: Place matches user digit scaling
- **WHEN** an operator opens the same console in Place at the same viewport size as the simple-user page
- **THEN** corresponding arc gauges show the same center digit font size

### Requirement: Place and user honor content align
The simple-user control page and Place view-mode MUST apply each binding’s `alignV` for Default, Icon, and Gauge view styles. When `showTitle` is false, they MUST also apply `alignH`. When `showTitle` is true with title position left or right, the row MUST keep name and control on one line (control opposite the title). For icon view with horizontal align `center` (including composite cell center when that applies), the icon MUST be visually centered in the cell with no right-side bias. Edit-mode preview SHOULD reflect the same layout without sending device commands. When Show title is unchecked in edit mode, the plate MUST show a muted registered device name (not the binding Title and not the IEEE).

#### Scenario: User page default view right align without title
- **WHEN** a simple user opens a console whose Default-view binding has `showTitle` false and `alignH` `right`
- **THEN** the binding content is right-aligned in its cell or slot

#### Scenario: Titled row keeps control on the same line
- **WHEN** a simple user opens a console whose binding has `showTitle` true and `titlePosition` `left`
- **THEN** the title and switch or icon appear on one row

#### Scenario: Edit preview shows muted device name
- **WHEN** an editor views a binding with `showTitle` false in the Consoles edit dialog
- **THEN** the plate shows the registered device name in muted text

#### Scenario: Centered engine icon looks centered
- **WHEN** a simple user opens a composite cell with an `mdi:engine` icon, no title, and horizontal align center
- **THEN** the icon sits visually in the horizontal center of the cell

## ADDED Requirements

### Requirement: Gauge range color picker presets
The Consoles binding dialog gauge color picker MUST offer eight preset swatches the operator can apply in one click: white, gray, light gray, red, green, light blue, yellow, and orange. Choosing a preset MUST set the active color for adding or editing a range. A free-form color control MAY remain available beside the presets.

#### Scenario: Preset fills range color
- **WHEN** the editor opens Gauge ranges and clicks the light-blue preset
- **THEN** the active range color becomes that light-blue value

#### Scenario: All eight presets present
- **WHEN** the editor views the gauge color picker presets
- **THEN** white, gray, light gray, red, green, light blue, yellow, and orange are all available

### Requirement: Console export filename uses transliterated name
When the Consoles editor exports a console as JSON, the browser download filename MUST be derived from that console’s name with non-Latin letters transliterated to Latin and unsafe path characters replaced by `_`, ending in `.json`.

#### Scenario: Latin name kept
- **WHEN** the editor exports a console named `Boiler`
- **THEN** the download filename is `Boiler.json` (or the same slug with only unsafe characters replaced)

#### Scenario: Cyrillic name transliterated
- **WHEN** the editor exports a console named `Котел`
- **THEN** the download filename uses a Latin transliteration such as `Kotel.json` instead of collapsing to underscores only
