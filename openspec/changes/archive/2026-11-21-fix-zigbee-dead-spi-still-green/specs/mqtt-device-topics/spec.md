# Spec Delta: mqtt-device-topics

## ADDED Requirements

### Requirement: Born interval always drives Zigbee status collect
The host MUST run the Zigbee status-collect path for registered `zigbee` devices on every configured born interval (`bornIntervalMin`), while in normal operation, **even when the server-born MQTT topic is empty**. That collect MUST request status the same way as today’s server-born Zigbee collect. Missing end-device replies MUST NOT by themselves mean slave failure (slave health is deferred-apply lost on the slave, per `zigbee-slave-radio`).

#### Scenario: Empty topic still collects on interval
- **WHEN** server-born topic is empty and the born interval elapses in normal operation
- **THEN** the host starts Zigbee status collect for registered Zigbee devices

#### Scenario: Interval collect excludes WiFi-only devices
- **WHEN** the interval collect runs and the map has both `zigbee` and `mqtt` devices
- **THEN** only `zigbee` devices are included in the Zigbee status collect

## MODIFIED Requirements

### Requirement: MQTT settings store server-born topic and interval
The settings SHALL store a server-born topic string and a born interval in minutes. The interval SHALL clamp to 5–120 inclusive; the factory default SHALL be 20. The server-born topic MAY be empty. Empty topic SHALL disable MQTT born listen and MQTT born publish. Empty topic MUST NOT disable the independent Zigbee interval status collect. Settings GET/POST, export, and restore SHALL include both fields.

#### Scenario: Default interval
- **WHEN** settings are initialized to defaults
- **THEN** born interval is 20 and server-born topic is empty

#### Scenario: Empty topic disables MQTT born only
- **WHEN** server-born topic is empty and MQTT is connected
- **THEN** the host does not subscribe for MQTT born and does not publish MQTT born announces, but still runs Zigbee status collect on the born interval

### Requirement: Server-born listen refreshes Zigbee states
When the server-born topic is non-empty and MQTT is connected, the host SHALL subscribe to that exact topic. On receipt of any payload on that topic, the host SHALL collect states of all registered `zigbee` devices the same way it does at boot (status reads that lead to state publishes when replies arrive). This MQTT-triggered collect is **in addition to** the independent born-interval Zigbee collect. The host SHALL NOT require a particular payload body. The host MUST NOT run that full Zigbee status-collect pass nested inside the same call that performs the entire device-topic subscribe/unsubscribe burst for session attach; status collect MAY start on a later service-loop iteration after born is handled.

#### Scenario: Any born message
- **WHEN** MQTT delivers any message on the configured server-born topic
- **THEN** the host starts a Zigbee status collect for registered Zigbee devices

#### Scenario: Topic path does not replace interval path
- **WHEN** server-born topic is set and the born interval also elapses
- **THEN** Zigbee status collect still runs on the interval as well as on MQTT born messages
