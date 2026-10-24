# Spec Delta

## ADDED Requirements

### Requirement: Custom command execute uses transport command path
When the host executes a registered device custom command, it MUST use the same channels mapping as other outbound commands for that device. For `mqtt` transport it MUST publish the custom-command payload to the mapped command topic and MUST NOT send Zigbee frames for that execute. For `zigbee` transport it MUST apply the custom-command body on the Zigbee device-command path.

#### Scenario: MQTT custom command publishes only
- **WHEN** the host executes custom command `reboot` on an `mqtt` device
- **THEN** the host publishes `reboot` on that device’s mapped command topic and does not send Zigbee on/off or ZCL frames for that execute

#### Scenario: Zigbee custom command uses body path
- **WHEN** the host executes custom command `scene` with value `hall` on a `zigbee` device
- **THEN** the host applies body `cmd=scene,val=hall` through the Zigbee device-command path for that device’s mapped channel
