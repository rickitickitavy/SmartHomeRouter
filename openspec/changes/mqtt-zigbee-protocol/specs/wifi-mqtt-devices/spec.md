# Spec Delta

## MODIFIED Requirements

### Requirement: WiFi devices publish state; host listens

A registered device with transport `mqtt` SHALL report runtime state by publishing to its stored state topic (and channel variants per channels mapping when `zigbeeProtocol` is false). When `zigbeeProtocol` is true, the device SHALL publish status only to the exact stored state topic using Zigbee-shaped bodies (`userData` uses cluster `0xFC00`). The host SHALL subscribe to those state topics and update last-known status for the console from inbound MQTT payloads. The host SHALL NOT publish Zigbee-originated state to an `mqtt` device’s state topic.

#### Scenario: State message updates console

- **WHEN** an `mqtt` device with channels `1` and `zigbeeProtocol` false publishes `ON` to its stored state topic
- **THEN** that device’s registered list JSON includes a status entry reflecting `ON`

#### Scenario: Zigbee-protocol state updates console

- **WHEN** an `mqtt` device with `zigbeeProtocol` true publishes `cl=0x0006,attr=0x0000,val=0x1,ep=1` to its stored state topic
- **THEN** that device’s registered list JSON includes a status entry for endpoint 1 reflecting that report

#### Scenario: Host does not invent WiFi state

- **WHEN** the host collects Zigbee statuses after a server-born message
- **THEN** it does not publish synthetic state to any `mqtt` device’s state topic as part of that collect

### Requirement: WiFi commands use MQTT set topics

For an `mqtt` device with `zigbeeProtocol` false, an inbound MQTT command on its set topic, Manual command, and type-action shortcuts SHALL be published to that device’s MQTT command topic with channels mapping applied. For an `mqtt` device with `zigbeeProtocol` true, those same paths SHALL publish Zigbee-shaped bodies to the exact stored command topic (including `ep` from the chosen channel; `userData` uses `cl=0xFC00`) without topic suffixes or `ch-<ep>##` wrapping. The host SHALL NOT send Zigbee on/off, write-attribute, or cluster commands on the radio for an `mqtt` device.

#### Scenario: Manual ON for WiFi

- **WHEN** Manual command sends `ON` for an `mqtt` device with `zigbeeProtocol` false and channels `1`
- **THEN** the host publishes `ON` to that device’s stored command topic and does not send a Zigbee on/off

#### Scenario: Manual ON for Zigbee-protocol WiFi

- **WHEN** Manual command sends `ON` for an `mqtt` device with `zigbeeProtocol` true and channel 1
- **THEN** the host publishes a Zigbee-shaped on body including `ep=1` to the exact stored command topic and does not send a Zigbee radio command
