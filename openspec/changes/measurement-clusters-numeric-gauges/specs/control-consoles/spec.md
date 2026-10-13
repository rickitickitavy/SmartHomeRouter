## MODIFIED Requirements

### Requirement: Numeric binding view style and gauge
For a device binding whose selected parameter is numeric, the binding MUST support view style `default` (current text/control presentation) or `gauge`. Numeric parameters MUST include: `sensorValue` for Measurement-class device types (at least temperature, pressure, humidity, illuminance, flow, metering, and electrical measurement), `battery`, `position`, `lightLevel`, and `custom` when the operator intends a numeric attribute. Missing view style MUST behave as `default`. When view style is `gauge`, the binding MUST store gauge type `arc` (default) or `linear`, numeric min and max, and an ordered list of color ranges. Each range MUST have a top limit and a color; the bottom of the first range MUST be the gauge min; each later range’s bottom MUST be the previous range’s top. Non-numeric bindings (On/Off status, covering move status, automation) MUST ignore gauge settings and keep their existing controls.

#### Scenario: Temperature sensor value allows gauge
- **WHEN** an editor saves a temperature Measurement binding with param `sensorValue`, view style `gauge`, type `arc`, min 0, max 40, and one range top 25 with a color
- **THEN** after reboot that binding still has those gauge fields

#### Scenario: Missing view style defaults to default
- **WHEN** a numeric binding has no viewStyle field
- **THEN** view mode uses the existing text or control presentation

#### Scenario: On/Off ignores gauge
- **WHEN** an On/Off binding has view style fields set in stored JSON
- **THEN** view mode shows the On/Off switch even if gauge fields are present in stored JSON

## ADDED Requirements

### Requirement: Gauge value uses measurement engineering units
When a Measurement-class binding is rendered as a gauge, the gauge needle and numeric scale MUST use the same engineering-unit value that the default measurement text display would use for that binding (including the binding’s selected unit when applicable). The gauge MUST NOT treat Measurement-class readings as non-numeric solely because the live state string includes units or formatting. The visible gauge value text MUST be the number only and MUST NOT append a measure unit suffix (for example °C, °F, K, hPa, %, lx).

#### Scenario: Temperature gauge tracks Celsius reading
- **WHEN** a temperature Measurement binding with view style `gauge` and unit Celsius shows live state that formats as a temperature reading
- **THEN** the gauge position reflects that Celsius number within the configured min and max

#### Scenario: Gauge readout omits units
- **WHEN** a temperature Measurement gauge shows a live reading of 22.5 °C in default text style terms
- **THEN** the gauge value text is `22.5` with no unit suffix

### Requirement: Arc gauge value font size
The arc gauge center value text MUST use a font size 15% smaller than the previous default of 16px (13.6px). Linear gauge value text size is unchanged by this requirement.

#### Scenario: Arc value is smaller
- **WHEN** a binding uses view style gauge with type arc
- **THEN** the center value text uses the reduced arc font size
