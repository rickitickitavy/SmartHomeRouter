# zigbee-device-rebind Specification

## Purpose

Lets operators rebind an already-registered Zigbee device after settings restore without deleting the host registry entry, using immediate ZDO bind first and a pairing-window fallback when that fails.

## Requirements

### Requirement: Rebind keeps the registered device record
Rebind of a registered Zigbee device SHALL NOT delete, replace, or clear that IEEE’s registered host record (name, channels, topics, flags, reportable data, and other persisted fields). MQTT-transport devices SHALL NOT be eligible for Zigbee rebind.

#### Scenario: Registry survives successful rebind
- **WHEN** the operator rebinds a registered Zigbee IEEE and the immediate bind succeeds
- **THEN** that IEEE remains registered with the same persisted fields as before the rebind

#### Scenario: MQTT device rejected
- **WHEN** the operator requests rebind for a registered MQTT-transport device
- **THEN** the system rejects the request and does not open Zigbee pairing for that IEEE

### Requirement: Immediate ZDO bind is attempted first
When rebind is requested for a registered Zigbee IEEE and the slave has a usable NWK short for that IEEE, the system SHALL first attempt a coordinator-initiated ZDO bind for that device. Immediate rebind SHALL apply to every Zigbee device kind that uses the shared join/bind path, not only On/Off.

#### Scenario: Reachable device binds without Search
- **WHEN** a registered Zigbee device has a usable short on the slave and the operator requests rebind
- **THEN** the system attempts immediate ZDO bind without requiring the operator to delete the device

#### Scenario: All kinds use the same rebind entry
- **WHEN** the operator rebinds a registered measurement or covering Zigbee device that has a usable short
- **THEN** the system uses the same rebind request path as for an On/Off device

### Requirement: Pairing-window fallback after immediate failure
When immediate rebind cannot start (no usable short/endpoint) or the ZDO bind fails, the system SHALL fall back to a pairing window: clear that IEEE from the coordinator switch bind cache so a re-announce can bind, open permit-join, and keep the registered record. The operator-facing outcome SHALL indicate that pairing fallback is active.

#### Scenario: No short falls back to pairing
- **WHEN** the operator requests rebind for a registered Zigbee IEEE with no usable short on the slave
- **THEN** the system opens permit-join for rebind fallback and does not delete the registered record

#### Scenario: Failed ZDO bind falls back to pairing
- **WHEN** immediate ZDO bind for a registered Zigbee IEEE fails
- **THEN** the system opens permit-join for rebind fallback and reports that fallback to the operator

### Requirement: Host exposes a rebind API for registered Zigbee devices
The host SHALL accept an authenticated rebind request that identifies a registered Zigbee IEEE. The response SHALL distinguish immediate success, pairing fallback, and hard failure (unknown IEEE, MQTT transport, or slave not ready). Only users who may edit devices SHALL be allowed to invoke rebind.

#### Scenario: Unknown IEEE rejected
- **WHEN** a rebind request names an IEEE that is not registered
- **THEN** the host rejects the request without opening permit-join

#### Scenario: Immediate success reported
- **WHEN** the slave completes immediate bind for the requested IEEE
- **THEN** the host response indicates immediate success


### Requirement: Rebind routes to the device router
When rebind is requested for a registered Zigbee device, the host SHALL use that device’s `router_id`. Value `0` SHALL use the local slave rebind path. A non-zero value SHALL send interconnect `rebind` only to the matching paired Repeater and wait for `rebind_result` with the same outcome classes as local rebind (immediate success, pairing fallback, failure). Rebind SHALL NOT open search or rebind on other interconnect peers. Rebind SHALL still not delete the registered record and SHALL still apply to all Zigbee device kinds on that path.

#### Scenario: Remote device rebinds on repeater
- **WHEN** the operator rebinds a registered Zigbee IEEE with router_id matching a paired Repeater
- **THEN** that Repeater performs local rebind and Main reports the returned outcome without removing the registry entry

#### Scenario: Other routers untouched
- **WHEN** rebind is requested for a device on one paired Repeater
- **THEN** Main does not send `rebind` or `search_start` to other paired Repeaters for that request

#### Scenario: Local device unchanged path
- **WHEN** the operator rebinds a registered Zigbee IEEE with router_id 0
- **THEN** the host uses the existing local SPI rebind path

### Requirement: Rebind wait is thirty seconds
The host rebind API SHALL wait up to **30** seconds for a local SPI rebind outcome or a remote `rebind_result` before reporting failure. The registered device record SHALL remain either way.

#### Scenario: Local wait thirty seconds
- **WHEN** local rebind does not complete within 30 seconds
- **THEN** the API reports failure and the IEEE stays registered

#### Scenario: Remote wait thirty seconds
- **WHEN** no rebind_result arrives within 30 seconds after sending rebind to a Repeater
- **THEN** the API reports failure and the IEEE stays registered
