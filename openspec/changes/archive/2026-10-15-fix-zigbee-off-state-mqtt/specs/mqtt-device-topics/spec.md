# Spec Delta

## ADDED Requirements

### Requirement: Zigbee On/Off state publishes both ON and OFF
When the host receives a Zigbee-sourced On/Off state report for a registered device and MQTT is connected, it SHALL publish that report on the mapped state topic with payload `ON` or `OFF` matching the report. A rapid sequence that includes both values SHALL produce a publish for each accepted report, including `OFF` after `ON` within a few seconds.

#### Scenario: OFF after rapid toggle
- **WHEN** a registered Zigbee `onOff` device with channels `1` reports `ON` then `OFF` on endpoint 1 and MQTT is connected
- **THEN** the host publishes `ON` then `OFF` on that device’s stored state topic

#### Scenario: Suffix mode OFF after ON
- **WHEN** a registered Zigbee device with channels `4` reports `ON` then `OFF` from endpoint 3 and MQTT is connected
- **THEN** the host publishes `ON` then `OFF` on `{storedStateTopic}/3`
