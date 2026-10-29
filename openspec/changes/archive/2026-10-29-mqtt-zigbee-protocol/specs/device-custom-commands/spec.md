# Spec Delta

## MODIFIED Requirements

### Requirement: MQTT transport execute format
For a device with transport `mqtt` and `zigbeeProtocol` false, executing a custom command MUST publish to that device’s mapped command topic. The payload MUST be `${command}` when no parameter is required, or `${command}=${value}` when a parameter is required, using the encoded value rules for that type. For a device with transport `mqtt` and `zigbeeProtocol` true, executing a custom command MUST publish to the exact stored command topic. The payload MUST be `cmd=${command}` when no parameter is required, or `cmd=${command},val=${value}` when a parameter is required, and MUST include `ep` for the chosen channel. The host MUST NOT invent a `val=` key when no parameter is required.

#### Scenario: MQTT command only
- **WHEN** MQTT device `lamp` runs custom command `reboot` with no parameter and `zigbeeProtocol` is false
- **THEN** the host publishes payload `reboot` on that device’s command topic

#### Scenario: MQTT command with value
- **WHEN** MQTT device runs custom command `level` with int parameter `40` and `zigbeeProtocol` is false
- **THEN** the host publishes payload `level=40` on that device’s command topic

#### Scenario: MQTT Zigbee-protocol command only
- **WHEN** MQTT device runs custom command `reboot` with no parameter, `zigbeeProtocol` true, and channel 1
- **THEN** the host publishes a body containing `cmd=reboot` and `ep=1` on the exact stored command topic

#### Scenario: MQTT Zigbee-protocol command with value
- **WHEN** MQTT device runs custom command `level` with int parameter `40`, `zigbeeProtocol` true, and channel 2
- **THEN** the host publishes a body containing `cmd=level`, `val=40`, and `ep=2` on the exact stored command topic
