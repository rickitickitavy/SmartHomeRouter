# Spec Delta

## ADDED Requirements

### Requirement: Device sync carries last-known NWK short
`SpiCmdSetDevice` and the slave device dump SHALL include the registered device’s last-known NWK short as two little-endian bytes immediately after the persisted device-type byte. A shorter legacy entry that ends at the type byte SHALL be treated as last-known short `0`. Short `0` or `0xFFFF` SHALL mean unknown on the wire.

#### Scenario: Sync round-trips short
- **WHEN** a registered Zigbee device has last-known short `0xA369` and the host syncs that entry to the slave
- **THEN** the slave receives short `0xA369` for that IEEE in the sync entry

#### Scenario: Legacy sync without short
- **WHEN** a sync entry arrives that ends at the type byte with no short field
- **THEN** the receiver treats last-known short as `0` for that entry

## MODIFIED Requirements

### Requirement: Device sync carries persisted device type
`SpiCmdSetDevice` and the slave device dump SHALL include the stored device type as the byte immediately after the full-control/debug flags byte (the same offset as today’s type byte). When a last-known NWK short is present, it SHALL follow the type byte as specified by the last-known short sync requirement; the type byte SHALL NOT move. A shorter legacy entry that ends at the type byte SHALL leave type as parsed and short as `0`. A shorter legacy entry without a type field SHALL leave type as `unknown` unless the host already has a known type for that IEEE, in which case the host SHALL keep the known type.

#### Scenario: Dump round-trips type
- **WHEN** a registered device has type `windowCovering` and the slave dumps the registry
- **THEN** the host list after pull still has type `windowCovering`

#### Scenario: Type offset unchanged with short trailer
- **WHEN** a sync entry includes both type `onOff` and last-known short `0x1234`
- **THEN** the type byte is still at the pre-short offset and decodes as `onOff`
