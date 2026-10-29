# Spec Delta

## MODIFIED Requirements

### Requirement: Device bindings match device class
An editor MUST add a device from registered devices, pick a channel when the device has more than one channel, and set a row title. When the selected binding parameter is a measurement-like reading, the editor MUST pick a unit from a list for that **parameter’s** measure kind (temperature, pressure, humidity, illuminance, flow, battery percent, and similar)—not from the device’s global classified type alone. On/Off rows MUST show a switch. Window covering rows MUST show up, down, stop, and current position. Sensor rows MUST show the live value in the chosen unit. An editor MUST also be able to add an automation binding from stored automations; that row MUST show an On/Off control reflecting the automation’s `active` flag (On = active).

#### Scenario: Multi-channel On/Off
- **WHEN** the editor adds a two-channel On/Off device and selects channel 2
- **THEN** the row controls only channel 2

#### Scenario: Automation row shows active as On/Off
- **WHEN** a widget binds automation `timer-light-on` that is active
- **THEN** the row switch shows On

#### Scenario: Temperature unit
- **WHEN** the editor adds a temperature sensor and selects the temperature / Sensor value parameter
- **THEN** the unit list includes Celsius, Fahrenheit, and Kelvin

#### Scenario: Battery param is not temperature units
- **WHEN** the editor binds a Measurement-class device and selects Battery level
- **THEN** the unit list is percent-oriented (or empty dash) and MUST NOT offer Celsius, Fahrenheit, or Kelvin as the only applicable set

#### Scenario: Humidity reportable on multi-value device
- **WHEN** the editor binds a device that also reports temperature and selects a humidity reportable parameter
- **THEN** the unit list is humidity percent, not temperature Celsius/Fahrenheit/Kelvin

### Requirement: Numeric binding view style and gauge
For a device binding whose selected parameter is numeric, the binding MUST support view style `default` (current text/control presentation) or `gauge`. Numeric parameters MUST include: `sensorValue` for Measurement-class device types (at least temperature, pressure, humidity, illuminance, flow, metering, and electrical measurement), `battery`, `position`, `lightLevel`, and `custom` when the operator intends a numeric attribute. Missing view style MUST behave as `default`. When view style is `gauge`, the binding MUST store gauge type `arc` (default) or `linear`, numeric min and max, and an ordered list of color ranges. Each range MUST have a top limit and a color; the bottom of the first range MUST be the gauge min; each later range’s bottom MUST be the previous range’s top. When gauge type is `linear`, the binding MUST also store one linear style among `bar`, `filled`, `vertical`, and `verticalFilled` (missing or invalid MUST behave as `bar`). Arc gauges MUST ignore linear style. Non-numeric bindings (On/Off status, covering move status, automation) MUST ignore gauge settings and keep their existing controls.

#### Scenario: Temperature sensor value allows gauge
- **WHEN** an editor saves a temperature Measurement binding with param `sensorValue`, view style `gauge`, type `arc`, min 0, max 40, and one range top 25 with a color
- **THEN** after reboot that binding still has those gauge fields

#### Scenario: Missing view style defaults to default
- **WHEN** a numeric binding has no viewStyle field
- **THEN** view mode uses the existing text or control presentation

#### Scenario: On/Off ignores gauge
- **WHEN** an On/Off binding has view style fields set in stored JSON
- **THEN** view mode shows the On/Off switch even if gauge fields are present in stored JSON

#### Scenario: Linear style persists
- **WHEN** an editor saves a linear gauge binding with linear style `verticalFilled`
- **THEN** after reboot that binding still has gauge type `linear` and linear style `verticalFilled`

#### Scenario: Missing linear style defaults to bar
- **WHEN** a stored linear gauge has no linear style field
- **THEN** view mode draws the existing horizontal growing-fill bar

### Requirement: Gauge appearance and scaling
In view mode, a gauge binding MUST render a Home Assistant–like arc or linear gauge using the configured min, max, value, color ranges, and (for linear) linear style. The gauge MUST scale to fit its viewport cell (grid or composite) or vertical row without overflowing the widget content box. Values outside min/max MUST clamp to the scale ends for drawing; the numeric readout MAY still show the raw value. Linear styles `filled` and `verticalFilled` MUST draw a full range-colored band (same range colors as the arc) as a straight track with a small value arrow, not the growing single-color fill bar. Styles `vertical` and `verticalFilled` MUST orient the track vertically and MUST use the maximum available height of the binding’s container after reserving space for the binding label (title) placement.

#### Scenario: Arc fits grid cell
- **WHEN** a grid viewport cell shows an arc gauge
- **THEN** the gauge fits inside that cell without overlapping neighboring cells

#### Scenario: Linear fits horizontal slot
- **WHEN** a composite viewport cell shows a linear gauge binding with style `bar` or `filled`
- **THEN** the bar scales to the cell width and stays within the cell

#### Scenario: Filled linear matches arc coloring with arrow
- **WHEN** a linear gauge uses style `filled` with the same ranges as an arc gauge
- **THEN** the track shows the full range-colored band straightened and a small arrow at the live value

#### Scenario: Vertical linear uses max height minus label
- **WHEN** a linear gauge uses style `vertical` or `verticalFilled` in a cell or row that also shows a binding title
- **THEN** the track uses the maximum remaining container height after the title’s space and does not overflow the container

## ADDED Requirements

### Requirement: Binding unit follows selected parameter
For a device binding, the unit field visibility and option list MUST be derived from the currently selected parameter (Sensor value using that device’s measurement class, reportable cluster-mapped measure kind, battery, or other property with a known unit set). Changing the parameter MUST refresh the unit options. A unit appropriate only for another property on the same device MUST NOT remain the sole applicable choice for the newly selected parameter.

#### Scenario: Switch param from temperature to humidity
- **WHEN** a binding’s unit list shows temperature units for Sensor value / temperature and the editor changes param to a humidity reading on the same device
- **THEN** the unit options refresh to humidity percent before Save

#### Scenario: Non-measurement param hides engineering units
- **WHEN** the editor selects a non-measurement binding parameter such as On/Off Status
- **THEN** the measurement unit field is hidden
