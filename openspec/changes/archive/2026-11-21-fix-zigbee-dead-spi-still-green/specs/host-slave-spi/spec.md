# Spec Delta: host-slave-spi

## ADDED Requirements

### Requirement: Host still resets when Ping clears Zigbee started
When the host previously saw slave Zigbee as started and a later Pong clears `SPI_STATUS_FLAG_ZIGBEE_STARTED` (including after the slave marked Zigbee lost because deferred radio apply failed), the host MUST reset the slave using the existing lost-Zigbee reset path. This change MUST NOT add a second SPI keepalive or Ping health service on the host.

#### Scenario: Slave apply-lost clears bit
- **WHEN** the slave has marked Zigbee lost after deferred radio apply failure and Ping reports Zigbee not started
- **THEN** the host resets the slave via the existing Zigbee-lost reset path

### Requirement: Search waits for applied permit-join result
When the host starts device Search or opens/closes permit-join on the local slave, it MUST treat the operation as started only after the slave reports applied success, not merely because the SPI command was enqueued. On applied failure or timeout (design: 3–5 s) while Zigbee was believed up, the host MUST NOT claim Search started, MUST log the failure, and MUST reset the slave via the existing lost-Zigbee path.

#### Scenario: False enqueue is not Search started
- **WHEN** permit-join is only enqueued on SPI and no applied success has arrived
- **THEN** the host does not treat Search as successfully started
