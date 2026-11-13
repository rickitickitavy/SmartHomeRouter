# Spec Delta

## Purpose

Lets operators rebind an already-registered Zigbee device after settings restore without deleting the host registry entry, using immediate ZDO bind first and a pairing-window fallback when that fails.

## ADDED Requirements

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
