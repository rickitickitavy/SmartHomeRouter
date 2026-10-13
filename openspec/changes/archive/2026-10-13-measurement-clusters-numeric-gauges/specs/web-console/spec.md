## MODIFIED Requirements

### Requirement: Console device dialog gauge config
When the selected binding parameter is numeric (including Measurement-class `sensorValue`, battery, position, light level, or custom) and the device is not a binary sensor type, the binding dialog MUST show a View style control with Default and Gauge. When Gauge is selected, the dialog MUST show a Gauge group: type Arc (default) or Linear, Min value, Max value, and a list of color ranges. Each list row shows top limit and color; the dialog MUST provide Top and color-picker fields below the list to append a range. Bottom of each range is derived (first from Min, later from previous top). View style Gauge MUST be hidden or ignored for On/Off status, covering move status, automation bindings, and binary sensor types.

#### Scenario: Temperature offers Gauge
- **WHEN** an editor opens a device binding for a temperature Measurement device and the param is Sensor value
- **THEN** the dialog shows View style with Default and Gauge

#### Scenario: On/Off hides Gauge
- **WHEN** an editor selects an On/Off device with param Status
- **THEN** View style Gauge is hidden

## ADDED Requirements

### Requirement: Console device dialog binary icon View style
When the selected device is a binary sensor type (Binary Input / Output / Value or BACnet binary), the binding dialog MUST show View style with Default and Icon (not Gauge). When Icon is selected, the dialog MUST show an icon picker of Material Design Icons (Home Assistant / Pictogrammers), including at least door, window, motion, water, pump, water-pump, heat-pump, and engine icons.

#### Scenario: Binary offers Icon
- **WHEN** an editor opens a binding for a binaryInput device
- **THEN** View style shows Default and Icon and does not show Gauge

#### Scenario: Pick pump icon
- **WHEN** the editor selects View style Icon and chooses `mdi:pump`
- **THEN** the saved binding stores `viewStyle` `icon` and `icon` `mdi:pump`

### Requirement: Measurement binding param defaults to Sensor value
When adding or refreshing a console device binding for a Measurement-class device (temperature, pressure, humidity, illuminance, flow, metering, electrical measurement, and the same set the console already treats as measurement for display), the param picker MUST offer Sensor value as the primary reading and MUST NOT leave an unmatched Status option selected merely because the dialog defaulted to `status`. If the previous param is not in that device’s catalog, the dialog MUST select the first catalog parameter instead of injecting the unmatched value as selected.

#### Scenario: New temperature binding selects Sensor value
- **WHEN** an editor adds a device binding and chooses a temperature Measurement device
- **THEN** the param select shows Sensor value selected and View style Gauge is available

#### Scenario: Switching from On/Off to pressure drops orphan Status
- **WHEN** an editor changes the binding device from an On/Off device (param Status) to a pressure Measurement device
- **THEN** the param select does not keep Status selected and instead selects Sensor value (or another catalog param for that device)

### Requirement: Place and user gauges show unitless value text
On the simple-user control page and Place view-mode, gauge-styled bindings MUST show the numeric value without a measure unit suffix. Arc gauges MUST use the reduced center value font size from the control-consoles arc font requirement.

#### Scenario: User page temperature gauge text
- **WHEN** a simple user opens a console with a temperature arc gauge
- **THEN** the center value shows digits only (no °C) at the reduced arc font size
