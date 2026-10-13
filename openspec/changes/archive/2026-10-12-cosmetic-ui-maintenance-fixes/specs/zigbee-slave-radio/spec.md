# Spec Delta

## ADDED Requirements

### Requirement: ZigBee settings apply without host restart

When the host persists new Zigbee CHANNEL or PERMIT_JOIN_ON_BOOT_SEC values from the console, the host MUST push those settings to the radio slave and the slave MUST apply them without restarting the host/router. If the Zigbee coordinator is already running on the slave, the slave MUST still apply the new channel (and related settings) rather than ignoring a second settings push.

#### Scenario: Channel change while radio running

- **WHEN** the Zigbee coordinator is already running and the operator saves a new valid CHANNEL
- **THEN** the host does not restart and the slave radio uses the new channel

#### Scenario: Permit-join on boot stored

- **WHEN** the operator saves a new PERMIT_JOIN_ON_BOOT_SEC value
- **THEN** the value is persisted and delivered to the slave without a host restart
