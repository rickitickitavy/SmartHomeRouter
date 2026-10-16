# Spec Delta

## ADDED Requirements

### Requirement: User-data devices store ordered field schema

Each `userData` device SHALL persist ordered `userDataFields` with `nullable`, unique non-empty `name`, and `type` (`int`/`float`/`bool`/`string`). List order is attr `0x0001` value-slot order. Omitted fields on create store `[]`. Schema MUST round-trip in `devices.json`, list JSON, and device export/import.

#### Scenario: Save schema then reload

- **WHEN** the operator saves a `userData` device with two fields (nullable off, name `temp`, type `float`) then (nullable on, name `label`, type `string`)
- **THEN** after reboot list JSON for that IEEE includes those two fields in that order with the same nullable, name, and type

#### Scenario: Empty schema allowed

- **WHEN** the operator saves a new `userData` device with no fields
- **THEN** the record has type `userData` and an empty `userDataFields` list

## MODIFIED Requirements

### Requirement: Registered devices store cluster device type

Each registered device record SHALL store a device type of `unknown`, `onOff`, `iasZone`, `windowCovering`, or `userData`. Type is a persisted device setting: it SHALL appear in `devices.json`, in `SpiCmdSetDevice` / dump payloads for `zigbee` devices, and in list JSON after reboot. For `zigbee` devices, readonly UI SHALL prevent the operator from editing type; it SHALL NOT skip persistence. A new save from a found device SHALL copy the type from that found record. A save that omits type on a new record SHALL store `unknown`. For `zigbee` devices, after a record has a type other than `unknown`, later operator saves SHALL leave that type unchanged even if the request body includes a different type. If a `zigbee` stored type is `unknown` and the host later receives a join classification other than `unknown` for the same IEEE, the host SHALL update the stored type to that classification. The slave SHALL persist the same type on its device dump so a host pull cannot replace a known type with `unknown`. For `mqtt` devices, the operator SHALL be allowed to set and later change type among `unknown`, `onOff`, `iasZone`, `windowCovering`, and `userData`; join classification SHALL NOT overwrite an `mqtt` type; the host SHALL NOT require the slave to store `mqtt` types. Type `userData` SHALL map to cluster id `0xFC00`.

#### Scenario: Save from found list

- **WHEN** the operator saves a found device whose join type is `iasZone`
- **THEN** the registered record has type `iasZone`

#### Scenario: Operator cannot overwrite a known type

- **WHEN** a registered `zigbee` device already has type `onOff` and the operator saves parameters with a different type in the request
- **THEN** the record still has type `onOff`

#### Scenario: Unknown fills in later

- **WHEN** a registered `zigbee` IEEE is `unknown` and a later join classifies it as `windowCovering`
- **THEN** the registered record becomes `windowCovering`

#### Scenario: Operator sets WiFi type

- **WHEN** a registered `mqtt` device has type `unknown` and the operator saves type `onOff`
- **THEN** the record has type `onOff`

#### Scenario: Operator sets MQTT userData type

- **WHEN** a registered `mqtt` device has type `unknown` and the operator saves type `userData`
- **THEN** the record has type `userData`
