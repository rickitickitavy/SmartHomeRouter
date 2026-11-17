# Spec Delta

## ADDED Requirements

### Requirement: Rebind routes to the device router
When rebind is requested for a registered Zigbee device, the host SHALL use that device’s `router_id`. Value `0` SHALL use the local slave rebind path. A non-zero value SHALL send interconnect `rebind` to the matching paired Repeater and wait for `rebind_result` with the same outcome classes as local rebind (immediate success, pairing fallback, failure). Rebind SHALL still not delete the registered record and SHALL still apply to all Zigbee device kinds on that path.

#### Scenario: Remote device rebinds on repeater
- **WHEN** the operator rebinds a registered Zigbee IEEE with router_id matching a paired Repeater
- **THEN** that Repeater performs local rebind and Main reports the returned outcome without removing the registry entry

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
