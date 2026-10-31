# device-configure Specification

## Purpose

Defines which Zigbee device attributes are operator-configurable by manufacturer and type, and how the host loads those values live from the radio and writes only operator-changed values back—without storing config values on the registered device record.

## Requirements

### Requirement: Manufacturer enum covers common Zigbee brands
The system MUST define a manufacturer enum with values `unknown`, `tuya`, `aqara`, `philipsHue`, `ikea`, `sonoff`, `ledvance`, `innr`, `gledopto`, `sengled`, `thirdReality`, `heiman`, `legrand`, `schneider`, `namron`, and `develco`. Each value MUST have a stable integer id and a JSON id string. Brands not recognized MUST map to `unknown`.

#### Scenario: Known Tuya join name
- **WHEN** a join manufacturer string matches a Tuya pattern (for example `_TZ3000_` / `TuYa` / `Tuya`)
- **THEN** the mapped enum is `tuya`

#### Scenario: Known LEDVANCE join name
- **WHEN** a join manufacturer string matches LEDVANCE or OSRAM
- **THEN** the mapped enum is `ledvance`

#### Scenario: Unrecognized brand
- **WHEN** a join manufacturer string does not match any known brand mapper
- **THEN** the mapped enum is `unknown`

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

### Requirement: Catalog entry describes typed ZCL targets
Each catalog entry MUST declare a stable config id, human label, value type (`bool`, `enum`, `uint8`, `uint16`, `int8`, `int16`), optional enum labels/values, Zigbee endpoint policy (fixed or first usable), cluster id, attribute id, and ZCL data type for read/write. Manufacturer-specific attributes MUST be marked as such when required by the device class.

#### Scenario: Enum field exposes choices
- **WHEN** a catalog entry has type `enum`
- **THEN** it includes an ordered list of allowed labels with corresponding numeric values used on the wire

### Requirement: Device has configurable attributes when catalog non-empty
A registered Zigbee device MUST be treated as having configurable attributes when its stored manufacturer enum and type yield at least one catalog entry. MQTT-transport devices MUST NOT be treated as having Zigbee configurable attributes in this capability.

#### Scenario: Zigbee Tuya onOff qualifies
- **WHEN** a registered device has transport `zigbee`, manufacturer `tuya`, and type `onOff`
- **THEN** the device has configurable attributes

#### Scenario: MQTT device does not qualify
- **WHEN** a registered device has transport `mqtt`
- **THEN** the device does not have Zigbee configurable attributes

### Requirement: Configure load reads live values from the device
Opening Configure for a device MUST read each applicable catalog attribute from the live Zigbee device (via the host→slave ZCL read path). The host MUST NOT load those values from the persisted device record. Values that fail to read MUST be reported as unavailable for that attribute without inventing a stored default.

#### Scenario: Successful live read
- **WHEN** the operator opens Configure for a reachable Zigbee device with catalog entries
- **THEN** each entry’s current value is obtained from a device attribute read (or marked unavailable if the read fails)

#### Scenario: Not persisted
- **WHEN** Configure values have been displayed after a live read
- **THEN** those values are not written into the registered device list or `devices.json`

### Requirement: Configure save writes only changed attributes
Saving Configure MUST write only attributes whose operator-edited value differs from the value obtained at load (or from the last successful live baseline in that dialog session). Unchanged attributes MUST NOT be written. Writes MUST use the catalog’s cluster, attribute, data type, and value encoding. Save MUST fail clearly when the device is offline or a required write fails.

#### Scenario: One field edited
- **WHEN** the operator changes only power-on behavior and saves
- **THEN** the host writes that attribute and does not write other unchanged catalog attributes

#### Scenario: No changes
- **WHEN** the operator opens Configure and saves without edits
- **THEN** the host performs no attribute writes

### Requirement: Attribute types drive validation
Configure inputs MUST validate according to the catalog value type: booleans accept true/false, enums accept only listed choices, integers accept only values in the declared range when a range is defined. Invalid input MUST be rejected without writing any attributes in that save.

#### Scenario: Invalid enum rejected
- **WHEN** the operator submits an enum value not in the catalog choices
- **THEN** Save is rejected and no Zigbee writes occur for that request

### Requirement: Initial catalog coverage for researched brands
The initial catalog MUST include the researched configurable settings for Tuya, Aqara, Philips Hue, IKEA, and Sonoff for the classified types listed in design.md (primarily `onOff`, plus level where noted). Types with no researched config MAY have an empty catalog.

#### Scenario: Sonoff onOff includes power-on
- **WHEN** manufacturer is `sonoff` and type is `onOff`
- **THEN** the catalog includes power-on behavior as a configurable enum

#### Scenario: Tuya onOff includes power-on and indicator class settings
- **WHEN** manufacturer is `tuya` and type is `onOff`
- **THEN** the catalog includes power-on behavior and MAY include switch type and indicator/backlight mode

#### Scenario: Aqara onOff includes power-on class settings
- **WHEN** manufacturer is `aqara` and type is `onOff`
- **THEN** the catalog includes power-on behavior and MAY include operation mode
