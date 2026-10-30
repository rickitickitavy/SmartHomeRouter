# Spec Delta

## ADDED Requirements

### Requirement: Consoles binding unit options follow selected parameter
In the Consoles device binding dialog, Unit and Decimals MUST refresh from the selected parameter’s measure kind, not only from the device’s classified type. Params with no engineering unit set MUST hide the unit field.

#### Scenario: Temperature device battery param
- **WHEN** the editor opens a binding for a temperature Measurement device and selects Battery level
- **THEN** the unit control does not offer Celsius, Fahrenheit, and Kelvin as the temperature set for that param

#### Scenario: Reportable humidity on temperature-class device
- **WHEN** the editor selects a humidity reportable parameter on a device whose classified type is temperature Measurement
- **THEN** the unit options are humidity percent, not temperature units

#### Scenario: Param change refreshes units
- **WHEN** the editor changes the binding param from Sensor value (temperature) to Battery level
- **THEN** the unit field options update immediately without requiring Save or reopening the dialog

#### Scenario: Sensor value keeps device class units
- **WHEN** the editor selects Sensor value on a temperature Measurement device
- **THEN** the unit list includes Celsius, Fahrenheit, and Kelvin

### Requirement: Consoles binding linear style has four states
When the Consoles device binding dialog Gauge type is Linear, the dialog MUST show one Style control with exactly four choices: Bar, Filled, Vertical, and Vertical filled (persisted as `bar`, `filled`, `vertical`, `verticalFilled`). When Gauge type is Arc, that Style control MUST be hidden and MUST NOT be required to Save. Saving a linear gauge MUST store the selected style on the binding gauge config.

#### Scenario: Style shown for linear only
- **WHEN** the editor sets Gauge type to Linear
- **THEN** the Style control lists Bar, Filled, Vertical, and Vertical filled

#### Scenario: Style hidden for arc
- **WHEN** the editor sets Gauge type to Arc
- **THEN** the Style control is hidden

#### Scenario: Save vertical filled
- **WHEN** the editor saves Gauge type Linear and Style Vertical filled
- **THEN** after reload the dialog shows Linear and Vertical filled
