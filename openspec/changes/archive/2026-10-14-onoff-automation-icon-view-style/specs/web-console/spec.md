## MODIFIED Requirements

### Requirement: Console device dialog binary icon View style
When the selected target is a binary sensor type (Binary Input / Output / Value or BACnet binary), an On/Off device, or an automation, the binding dialog MUST show View style with Default and Icon (not Gauge). When Icon is selected, the dialog MUST show an icon picker of Material Design Icons (Home Assistant / Pictogrammers), including at least door, window, motion, water, pump, water-pump, heat-pump, and engine icons. Measurement numeric bindings MUST keep Default/Gauge and MUST NOT use this Icon View style path.

#### Scenario: Binary offers Icon
- **WHEN** an editor opens a binding for a binaryInput device
- **THEN** View style shows Default and Icon and does not show Gauge

#### Scenario: On/Off offers Icon
- **WHEN** an editor opens a binding for an On/Off device
- **THEN** View style shows Default and Icon

#### Scenario: Automation offers Icon
- **WHEN** an editor sets the binding target to automation
- **THEN** View style shows Default and Icon and the icon picker is available when Icon is selected

#### Scenario: Pick pump icon
- **WHEN** the editor selects View style Icon and chooses `mdi:pump`
- **THEN** the saved binding stores `viewStyle` `icon` and `icon` `mdi:pump`

### Requirement: User and Place pages render viewport and gauges
The simple-user control page and Place view-mode MUST lay out each widget’s bindings according to that widget’s viewport and MUST render gauge-styled numeric bindings as scaled arc or linear gauges and icon-styled binary, On/Off, and automation bindings as Material Design Icons. Edit-mode preview in the Consoles dialog SHOULD reflect the same viewport, gauge, and icon choices using display-only controls (no device commands).

#### Scenario: User page grid gauges
- **WHEN** a simple user opens a console whose widget uses grid viewport and arc gauges
- **THEN** bindings appear in the grid and gauges fit their cells

#### Scenario: Place matches user viewport
- **WHEN** an operator opens the same console in Place view mode
- **THEN** viewport and gauge presentation match the simple-user page for that layout

#### Scenario: User page binary icon
- **WHEN** a simple user opens a console with a binaryInput binding using view style icon
- **THEN** the binding shows the chosen MDI icon (not a switch)

#### Scenario: User page On/Off icon
- **WHEN** a simple user opens a console with an On/Off binding using view style icon
- **THEN** the binding shows the chosen MDI icon instead of a switch and the icon toggles the device when activated

#### Scenario: User page automation icon
- **WHEN** a simple user opens a console with an automation binding using view style icon
- **THEN** the binding shows the chosen MDI icon instead of a switch and the icon toggles automation active when activated
