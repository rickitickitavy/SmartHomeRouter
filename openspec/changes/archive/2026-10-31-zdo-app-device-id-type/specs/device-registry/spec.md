# Spec Delta

## MODIFIED Requirements

### Requirement: Registered devices store cluster device type

Each registered device record SHALL store a device type of `unknown`, `onOff`, `iasZone`, `windowCovering`, `userData`, or another catalog type id the gateway persists for Zigbee Application Device ID classification. Type is a persisted device setting: it SHALL appear in `devices.json`, in `SpiCmdSetDevice` / dump payloads for `zigbee` devices, and in list JSON after reboot. For `zigbee` devices, readonly UI SHALL prevent the operator from editing type; it SHALL NOT skip persistence. A new save from a found device SHALL copy the type from that found record. A save that omits type on a new record SHALL store `unknown`. For `zigbee` devices, after the record exists in the registry (any stored type including `unknown`), later joins, Simple Descriptor results, Basic identity reads, and operator saves SHALL leave that type unchanged—even if a later classification differs or the request body includes a different type. The host SHALL NOT update a registered Zigbee type from a later join classification. The slave SHALL persist the same type on its device dump so a host pull cannot replace a known type with `unknown`. For `mqtt` devices, the operator SHALL be allowed to set and later change type among `unknown`, `onOff`, `iasZone`, `windowCovering`, and `userData`; join classification SHALL NOT overwrite an `mqtt` type; the host SHALL NOT require the slave to store `mqtt` types. Type `userData` SHALL map to cluster id `0xFC00`.

#### Scenario: Save from found list

- **WHEN** the operator saves a found device whose join type is `iasZone`
- **THEN** the registered record has type `iasZone`

#### Scenario: Operator cannot overwrite a known type

- **WHEN** a registered `zigbee` device already has type `onOff` and the operator saves parameters with a different type in the request
- **THEN** the record still has type `onOff`

#### Scenario: Registered type stays after later join

- **WHEN** a registered `zigbee` IEEE has type `onOff` and a later join would classify Application Device ID as temperature sensor
- **THEN** the registered record remains `onOff`

#### Scenario: Registered unknown is not filled later

- **WHEN** a registered `zigbee` IEEE is `unknown` and a later join classifies it as `windowCovering`
- **THEN** the registered record remains `unknown`

#### Scenario: Operator sets WiFi type

- **WHEN** a registered `mqtt` device has type `unknown` and the operator saves type `onOff`
- **THEN** the record has type `onOff`

#### Scenario: Operator sets MQTT userData type

- **WHEN** a registered `mqtt` device has type `unknown` and the operator saves type `userData`
- **THEN** the record has type `userData`

## ADDED Requirements

### Requirement: Found devices take type from Application Device ID join
While search is active, a found (not yet registered) Zigbee entry’s type SHALL come from the slave join classification based on Application Device ID. Manufacturer and model on the found row SHALL NOT change that type.

#### Scenario: Found row matches Device ID class
- **WHEN** the slave reports a found join with Application Device ID mapped to `onOff`
- **THEN** the found list shows type `onOff` for that IEEE
