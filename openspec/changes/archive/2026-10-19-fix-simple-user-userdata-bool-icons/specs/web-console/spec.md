# Spec Delta

## MODIFIED Requirements

### Requirement: Console device dialog binary icon View style
When the selected target is a binary sensor type (Binary Input / Output / Value or BACnet binary), an On/Off device, an automation, or a `userData` device parameter whose schema field type is `bool`, the binding dialog MUST show View style with Default and Icon (not Gauge). When Icon is selected, the dialog MUST show an icon picker of Material Design Icons (Home Assistant / Pictogrammers), including at least door, window, motion, water, pump, water-pump, heat-pump, and engine icons. Measurement numeric bindings MUST keep Default/Gauge and MUST NOT use this Icon View style path. `userData` int/float/string params MUST NOT offer Icon.

#### Scenario: Binary offers Icon
- **WHEN** an editor opens a binding for a binaryInput device
- **THEN** View style shows Default and Icon and does not show Gauge

#### Scenario: On/Off offers Icon
- **WHEN** an editor opens a binding for an On/Off device
- **THEN** View style shows Default and Icon

#### Scenario: Automation offers Icon
- **WHEN** an editor sets the binding target to automation
- **THEN** View style shows Default and Icon and the icon picker is available when Icon is selected

#### Scenario: UserData bool offers Icon
- **WHEN** an editor opens a binding for a `userData` device and selects a schema field whose type is `bool`
- **THEN** View style shows Default and Icon and does not show Gauge

#### Scenario: Pick pump icon
- **WHEN** the editor selects View style Icon and chooses `mdi:pump`
- **THEN** the saved binding stores `viewStyle` `icon` and `icon` `mdi:pump`

### Requirement: User and Place pages render viewport and gauges
The simple-user control page and Place view-mode MUST lay out each widget’s bindings according to that widget’s viewport (`vertical`, `grid`, or `composite`) and MUST render gauge-styled numeric bindings as scaled arc or linear gauges and icon-styled binary, On/Off, automation, and `userData` bool bindings as Material Design Icons. Edit-mode preview in the Consoles dialog SHOULD reflect the same viewport, gauge, and icon choices using display-only controls (no device commands). Binding view styles MUST keep existing Default / Gauge / Icon behavior.

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

#### Scenario: User page userData bool icon
- **WHEN** a simple user opens a console with a `userData` bool field binding using view style icon
- **THEN** the binding shows the chosen Material Design Icon instead of ON/OFF text

#### Scenario: User page composite layout
- **WHEN** a simple user opens a console whose widget uses composite viewport with mixed cm and unit cells
- **THEN** cells size per the composite size rules and device cells show their bindings
