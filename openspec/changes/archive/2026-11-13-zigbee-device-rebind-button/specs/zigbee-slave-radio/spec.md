# Spec Delta

## ADDED Requirements

### Requirement: Slave immediate rebind uses known short and endpoints
When the slave receives a rebind request for a registered IEEE and has a usable NWK short for that IEEE, it SHALL attempt coordinator-initiated ZDO bind for that device’s usable endpoints. This SHALL cover every Zigbee device kind on the shared join/bind path. On success the slave SHALL refresh its bound-device identity for that IEEE the same way a successful join bind does.

#### Scenario: Immediate bind with stored short
- **WHEN** a registered IEEE has usable short `0xA369` and a usable endpoint and rebind is requested
- **THEN** the slave attempts ZDO bind for that IEEE without requiring a new Search delete cycle

#### Scenario: Success updates bound identity
- **WHEN** immediate ZDO bind succeeds for a registered IEEE
- **THEN** the slave can address that IEEE through its bound/registered control path without deleting the registry entry

### Requirement: Slave pairing fallback clears bind cache for the IEEE
When immediate rebind cannot start or fails, the slave SHALL remove that IEEE from the Arduino switch bind-device cache (so a later device announce is not skipped as already bound), SHALL open permit-join for a finite pairing window, and SHALL keep the IEEE in the registered-device map. A later successful announce/bind for that IEEE SHALL restore bound identity while leaving the registered record intact.

#### Scenario: Fallback opens join window
- **WHEN** immediate rebind fails for a registered IEEE
- **THEN** the slave opens permit-join and that IEEE remains registered

#### Scenario: Re-announce can bind again
- **WHEN** pairing fallback has cleared the switch bind cache for that IEEE and the device announces during the open window
- **THEN** the coordinator bind path can run again for that IEEE without the operator deleting the registered device
