# Spec Delta

## MODIFIED Requirements

### Requirement: Configuration catalog is keyed by manufacturer and type
The system MUST maintain a compile-time (`static const`, flash-resident) catalog of configurable attributes keyed by manufacturer enum, classified device type, and optional model prefix. Manufacturer and model (Basic Manufacturer Name / Model Identifier) MUST be used only to select which configurable attributes apply; they MUST NOT assign or override the device’s stored Zigbee type. A configurable attribute MUST exclude runtime telemetry and operational state such as on/off status, covering position, battery, RSSI, illuminance, temperature, humidity, and similar live values. Registered devices MUST persist the Zigbee model string used for matching.

#### Scenario: Tuya onOff has config entries
- **WHEN** manufacturer is `tuya` and type is `onOff`
- **THEN** the catalog includes at least power-on behavior, and MAY include switch type and indicator/backlight mode when those attributes apply to that class

#### Scenario: Tuya window covering has config entries
- **WHEN** manufacturer is `tuya` and type is `windowCovering` (for example model `TS130F`)
- **THEN** the catalog includes motor reversal and calibration settings and does not include runtime position/moving status

#### Scenario: Added brands onOff have curated power-on
- **WHEN** manufacturer is one of `ledvance`, `innr`, `gledopto`, `sengled`, `thirdReality`, `heiman`, `legrand`, `schneider`, `namron`, or `develco` and type is `onOff`
- **THEN** the catalog includes curated power-on behavior (`startUpOnOff`) and does not invent runtime status attributes

#### Scenario: Status is not configurable
- **WHEN** the catalog is queried for any manufacturer and type
- **THEN** on/off status, covering position, battery, and RSSI are absent from configurable attributes

#### Scenario: Model does not change type
- **WHEN** a registered device has type `onOff` and Basic model/manufacturer strings match a temperature-oriented product profile in the config catalog
- **THEN** Configure may show attributes for that manufacturer/model/type key but the stored device type remains `onOff`
