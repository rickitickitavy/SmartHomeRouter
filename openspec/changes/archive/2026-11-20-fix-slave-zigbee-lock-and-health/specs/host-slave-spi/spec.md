# Spec Delta: host-slave-spi

## ADDED Requirements

### Requirement: Ping Zigbee flag follows strengthened health
When the slave answers a host Ping after Zigbee has been started, the Pong `SPI_STATUS_FLAG_ZIGBEE_STARTED` bit MUST be set only when the slave’s Zigbee health check succeeds (Zigbee lock acquired and outbound buffer probe under that lock, per `zigbee-slave-radio`). When health fails, the slave MUST clear that started indication for the Pong and MUST follow the existing Zigbee-lost reboot path. The host MUST keep treating a transition from Zigbee-up to Zigbee-down on Ping as slave Zigbee lost and MUST reset the slave as it does today.

#### Scenario: Soft hang fails Ping Zigbee bit
- **WHEN** the Zigbee lock still acquires but the buffer-pool health probe fails
- **THEN** the Pong does not claim Zigbee started and the slave schedules Zigbee-lost recovery

#### Scenario: Host still resets on lost flag
- **WHEN** the host previously saw Zigbee started and a later Pong clears the Zigbee started flag
- **THEN** the host resets the slave using the existing lost-Zigbee reset path
