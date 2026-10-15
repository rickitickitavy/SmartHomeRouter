# Spec Delta

## ADDED Requirements

### Requirement: publishToMqtt gates Zigbee MQTT state publish
When a registered non-MQTT device has `publishToMqtt` false, the host MUST NOT publish that device’s state (or other payloads that use its stored state topic mapping) to MQTT even if a state topic string is present. When `publishToMqtt` is true and MQTT is connected, existing state-publish rules for that device SHALL apply.

#### Scenario: Flag off skips state publish
- **WHEN** a `zigbee` device with `publishToMqtt` false reports `ON` and MQTT is connected
- **THEN** the host does not publish to that device’s state topic

#### Scenario: Flag on publishes
- **WHEN** a `zigbee` device with `publishToMqtt` true reports `OFF` and MQTT is connected
- **THEN** the host publishes `OFF` on the mapped state topic as today

### Requirement: publishToMqtt gates device MQTT subscriptions
When a registered non-MQTT device has `publishToMqtt` false, the host MUST NOT subscribe to that device’s command topic (or channel suffix wildcards for that command topic) or otherwise create MQTT listeners for that device’s stored MQTT topics. When `publishToMqtt` is true, existing device command (and related) subscription rules SHALL apply. MQTT-transport device subscriptions are unchanged by this flag.

#### Scenario: Flag off no command subscribe
- **WHEN** MQTT connects and a `zigbee` device has `publishToMqtt` false and a non-empty command topic
- **THEN** the host is not subscribed to that command topic

#### Scenario: Flag on subscribes
- **WHEN** MQTT connects and a `zigbee` device has `publishToMqtt` true and command topic `z2m/lamp/set`
- **THEN** the host is subscribed to that command topic (and suffix wildcard when channels mapping requires it)
