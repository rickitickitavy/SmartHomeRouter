# Spec Delta: control-consoles

## ADDED Requirements

### Requirement: Widget viewport layout
Each widget MUST have a viewport of `vertical` (default), `horizontal`, or `grid`. Missing viewport on load MUST behave as `vertical`. When viewport is `grid`, the widget MUST store positive integer Columns and Rows (defaults **2** and **2** when missing or invalid). In view mode, bindings MUST lay out according to viewport: vertical stacks top-to-bottom; horizontal places bindings in one row; grid fills cells left-to-right then top-to-bottom for Columns × Rows. If there are more bindings than Columns × Rows, remaining bindings MUST continue in additional rows using the same column count. Controls and gauges inside a cell or horizontal slot MUST scale to fit that slot.

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
- **WHEN** a widget viewport is `horizontal` with three bindings
- **THEN** view mode places the three bindings in one horizontal row

### Requirement: Device binding parameter selection
A device binding (not an automation binding) MUST allow selecting the displayed or commanded parameter using the same fields and dependent behavior as automation “device to trigger”: registered device, channel when multi-channel, param from the device’s parameter list, and when param is `custom`, cluster and attrId (hex). Missing param on load MUST default to the current channel/status behavior used today for that device class. Automation bindings MUST NOT require these parameter fields.

#### Scenario: Custom attribute binding
- **WHEN** the editor saves a device binding with param `custom`, cluster, and attrId
- **THEN** after reload that binding still has those values and view mode shows that attribute’s value when available

#### Scenario: Default param omitted
- **WHEN** an older console binding has ieee and channel but no param
- **THEN** the host still loads it and view mode behaves as before this change

### Requirement: Numeric binding view style and gauge
For a device binding whose selected parameter is numeric (measurement class or custom numeric), the binding MUST support view style `default` (current text/control presentation) or `gauge`. Missing view style MUST behave as `default`. When view style is `gauge`, the binding MUST store gauge type `arc` (default) or `linear`, numeric min and max, and an ordered list of color ranges. Each range MUST have a top limit and a color; the bottom of the first range MUST be the gauge min; each later range’s bottom MUST be the previous range’s top. Non-numeric bindings (On/Off, covering, automation) MUST ignore gauge settings and keep their existing controls.

#### Scenario: Gauge config persists
- **WHEN** the editor saves a temperature binding with view style `gauge`, type `arc`, min 0, max 40, and one range top 25 with a color
- **THEN** after reboot that binding still has those gauge fields

#### Scenario: Default view style
- **WHEN** a numeric binding has no viewStyle field
- **THEN** view mode shows the value in the default text style

#### Scenario: On/Off ignores gauge
- **WHEN** a binding targets an On/Off device
- **THEN** view mode shows the On/Off switch even if gauge fields are present in stored JSON

### Requirement: Gauge appearance and scaling
In view mode, a gauge binding MUST render a Home Assistant–like arc or linear gauge using the configured min, max, value, and color ranges. The gauge MUST scale to fit its viewport cell (grid), horizontal slot, or vertical row without overflowing the widget content box. Values outside min/max MUST clamp to the scale ends for drawing; the numeric readout MAY still show the raw value.

#### Scenario: Arc fits grid cell
- **WHEN** a grid viewport cell shows an arc gauge
- **THEN** the gauge fits inside that cell without overlapping neighboring cells

#### Scenario: Linear fits horizontal slot
- **WHEN** a horizontal viewport shows a linear gauge binding
- **THEN** the bar scales to the slot width and stays within the widget

## MODIFIED Requirements

### Requirement: Consoles persist with widgets and devices
The host MUST persist control consoles. Each console MUST have a unique id, a name, an active flag, and an ordered list of widgets. Each widget MUST have a title, a show-group-on-off flag, a viewport (`vertical`, `horizontal`, or `grid`), optional grid Columns and Rows when viewport is `grid`, and an ordered list of bindings. A binding MUST target either a registered device (optional channel, title, optional unit, optional param/custom cluster/attrId, optional view style and gauge config) or an automation by `name` (optional row title). Empty consoles MUST be allowed. After reboot the same consoles MUST load. The host MUST allow at most 32 consoles. Device-only exports from older firmware (bindings without automation targets) MUST still load. Widgets and bindings missing the new fields MUST load with defaults: viewport `vertical`, view style `default`.

#### Scenario: Save and reload with automation binding
- **WHEN** an editor saves console `Kitchen` with one widget that binds automation `yard-light`
- **THEN** after reboot `Kitchen` still has that widget and automation binding

#### Scenario: Thirty-third console rejected
- **WHEN** the host already has 32 consoles and an editor saves a new one
- **THEN** the host does not persist the thirty-third console and returns an error

#### Scenario: Save and reload grid viewport and gauge
- **WHEN** an editor saves a widget with viewport `grid`, Columns 2, Rows 2, and a gauge binding
- **THEN** after reboot that widget still has those viewport and gauge settings
