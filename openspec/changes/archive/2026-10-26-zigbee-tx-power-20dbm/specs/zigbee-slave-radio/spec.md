# Spec Delta

## ADDED Requirements

### Requirement: Coordinator TX power is explicitly 20 dBm
After the slave Zigbee coordinator has started successfully for the current boot, the slave MUST set IEEE 802.15.4 transmit power to **+20 dBm** via the Zigbee stack TX-power API (`esp_zb_set_tx_power` or equivalent). The slave MUST NOT leave TX power solely to an implicit stack default. The slave SHOULD log the configured TX power (and read-back when the stack provides it) at info level. This requirement does not require a settings UI or a persisted power preference.

#### Scenario: Power set after successful start
- **WHEN** `Zigbee.begin` for the coordinator succeeds on the slave
- **THEN** the slave sets TX power to +20 dBm before coordinator start is reported as complete to the rest of the firmware

#### Scenario: Start failure leaves radio unused
- **WHEN** `Zigbee.begin` for the coordinator fails
- **THEN** the slave does not claim a successful TX-power configuration for that boot and does not start the coordinator

#### Scenario: Operator can see power in the log
- **WHEN** the coordinator has started and TX power has been set
- **THEN** the slave log includes an info line that states the TX power in dBm (including the value returned by a get API when available)
