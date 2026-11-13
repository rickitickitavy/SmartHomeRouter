# Spec Delta

## ADDED Requirements

### Requirement: Host can request Zigbee device rebind over SPI
The SPI protocol SHALL carry a host-to-slave rebind command that identifies the target IEEE. The slave SHALL attempt immediate ZDO bind for that IEEE when a usable short is known, otherwise SHALL enter pairing-window rebind fallback. The command SHALL NOT remove the IEEE from the registered-device map.

#### Scenario: Rebind frame carries IEEE
- **WHEN** the host enqueues a rebind for a registered Zigbee IEEE
- **THEN** the slave receives that IEEE on the rebind command path

#### Scenario: Registry map untouched by SPI rebind
- **WHEN** the slave handles a rebind command for a registered IEEE
- **THEN** that IEEE remains present in the slave registered-device map

### Requirement: Slave reports rebind outcome to the host
The SPI protocol SHALL carry a slave-to-host rebind result that identifies the IEEE and whether the outcome was immediate success, pairing fallback, or failure. The host SHALL use that result to answer the operator rebind API.

#### Scenario: Immediate success result
- **WHEN** the slave completes immediate ZDO bind for the requested IEEE
- **THEN** the host receives a rebind result of immediate success for that IEEE

#### Scenario: Pairing fallback result
- **WHEN** the slave opens permit-join because immediate rebind could not succeed
- **THEN** the host receives a rebind result of pairing fallback for that IEEE
