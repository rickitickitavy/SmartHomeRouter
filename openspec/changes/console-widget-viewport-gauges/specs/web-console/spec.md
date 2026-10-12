# Spec Delta: web-console

## ADDED Requirements

### Requirement: Console editor sets widget viewport
In the Consoles edit dialog, each widget MUST expose a Viewport control with options Vertical, Horizontal, and Grid (default Vertical). When Grid is selected, the dialog MUST show Columns and Rows integer fields defaulting to 2 and 2. Changing viewport or grid size MUST update the in-dialog preview layout without persisting until Save. Cancel or reload without Save MUST discard unsaved viewport edits.

#### Scenario: Grid fields appear
- **WHEN** the editor sets a widget Viewport to Grid
- **THEN** Columns and Rows fields are visible and default to 2 and 2 for a new grid selection without prior values

#### Scenario: Vertical hides grid fields
- **WHEN** the editor sets Viewport to Vertical
- **THEN** Columns and Rows fields are hidden

### Requirement: Console device dialog matches automation trigger fields
When adding or editing a device binding (not automation), the binding dialog MUST offer device, channel, and param selects with the same dependent refresh behavior as automation “Add device to trigger”, including custom cluster and attrId when param is `custom`. Existing title, unit, and decimals fields MUST remain where they apply. Automation target mode MUST keep the automation picker and MUST NOT show gauge or custom cluster fields.

#### Scenario: Param list follows device
- **WHEN** the editor picks a registered device in the binding dialog
- **THEN** the param list refreshes for that device the same way as automation listen

#### Scenario: Custom shows cluster and attr
- **WHEN** the editor sets param to `custom`
- **THEN** cluster and attrId fields become visible and are included in the saved binding body

### Requirement: Console device dialog gauge config
When the selected binding parameter is numeric (or custom), the binding dialog MUST show a View style control with Default and Gauge. When Gauge is selected, the dialog MUST show a Gauge group: type Arc (default) or Linear, Min value, Max value, and a list of color ranges. Each list row shows top limit and color; the dialog MUST provide Top and color-picker fields below the list to append a range. Bottom of each range is derived (first from Min, later from previous top). View style Gauge MUST be hidden or ignored for On/Off, covering, and automation bindings.

#### Scenario: Gauge group appears
- **WHEN** the editor chooses View style Gauge for a temperature binding
- **THEN** type, min, max, and the color-range list editor are visible

#### Scenario: Add color range
- **WHEN** the editor enters a Top and color and adds a range
- **THEN** that range appears in the list and is included when the binding is saved

### Requirement: User and Place pages render viewport and gauges
The simple-user control page and Place view-mode MUST lay out each widget’s bindings according to that widget’s viewport and MUST render gauge-styled numeric bindings as scaled arc or linear gauges. Edit-mode preview in the Consoles dialog SHOULD reflect the same viewport and gauge choices using display-only controls (no device commands).

#### Scenario: User page grid gauges
- **WHEN** a simple user opens a console whose widget uses grid viewport and arc gauges
- **THEN** bindings appear in the grid and gauges fit their cells

#### Scenario: Place matches user viewport
- **WHEN** an operator opens the same console in Place view mode
- **THEN** viewport and gauge presentation match the simple-user page for that layout
