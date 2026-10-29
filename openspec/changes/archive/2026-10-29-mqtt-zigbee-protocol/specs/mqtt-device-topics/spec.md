# Spec Delta

## ADDED Requirements

### Requirement: Zigbee protocol ignores channel topic mapping
When a registered `mqtt` device has `zigbeeProtocol` true, the host SHALL publish and subscribe only on the exact stored state and command topics. The host SHALL NOT apply topic suffix mapping or `ch-<ep>##` payload wrapping for that device, regardless of `channels`. Availability SHALL still use the stored availability topic with no suffix. `channels` MAY still describe how many endpoints the console and automations expose.

#### Scenario: Multi-channel uses base topics
- **WHEN** an `mqtt` device has `zigbeeProtocol` true, `channels` `4`, state `home/plug/state`, and command `home/plug/set`
- **THEN** the host subscribes to `home/plug/state` only (not `home/plug/state/#`) and publishes commands to `home/plug/set` only

#### Scenario: Parse mode channels ignored for topics
- **WHEN** an `mqtt` device has `zigbeeProtocol` true and `channels` `0`
- **THEN** inbound and outbound traffic still use the unsuffixed stored state and command topics without `ch-<ep>##` wrapping

### Requirement: Zigbee protocol MQTT uses ZCL-shaped bodies
When a registered `mqtt` device has `zigbeeProtocol` true, inbound status and outbound command MQTT bodies SHALL use Zigbee-style `key=value` fields (`cl`, `attr`, `val`, `ep`, and `cl`/`cmd` for cluster commands), accepting decimal or `0x` hex values. The host SHALL apply semantics from the packet’s `cl` (and `attr`), not from device class alone. Cluster `0xFC00` / attr `0x0001` is user-data (brace list in `val=`; device settings SHALL provide `userDataFields`). Power Configuration `0x0001` / Battery Percentage Remaining `0x0021` updates battery telemetry. On/Off, IAS, and window-covering clusters keep their existing mappings.

#### Scenario: Inbound attribute report
- **WHEN** that device publishes `cl=0x0006,attr=0x0000,val=0x1,ep=2` on its stored state topic
- **THEN** the host updates that device’s runtime status for endpoint 2 from that report

#### Scenario: Inbound default endpoint
- **WHEN** that device publishes `On/Off cl=0x0006,attr=0x0000,val=0x0` with no `ep=`
- **THEN** the host treats the report as endpoint 1

#### Scenario: Inbound battery by cluster
- **WHEN** an `mqtt` device with `zigbeeProtocol` true publishes `cl=0x0001,attr=0x0021,val=70,ep=1` on its state topic
- **THEN** the host updates that device’s battery telemetry to 70% and does not treat the packet as user-data `0xFC00`

#### Scenario: Inbound user-data only for 0xFC00
- **WHEN** that device publishes `cl=0xFC00,attr=0x0001,val={a,ON},ep=1` on its state topic
- **THEN** the host applies the brace list to user-data status using the device’s `userDataFields` schema

#### Scenario: Outbound encodes ON with endpoint
- **WHEN** Manual command or a type action sends `ON` for channel 3 on that device
- **THEN** the host publishes a Zigbee-shaped on body including `ep=3` to the exact stored command topic

#### Scenario: Already-shaped body passes through
- **WHEN** Manual command sends `cl=0x0102,cmd=0x02,ep=1` for that device
- **THEN** the host publishes that body to the exact stored command topic without `ch-` wrapping or a topic suffix

#### Scenario: userData brace outbound uses cluster 0xFC00
- **WHEN** Manual command sends `{21.5,ON}` for an `mqtt` `userData` device with `zigbeeProtocol` true and channel 1
- **THEN** the host publishes a body containing `cl=0xFC00`, `attr=0x0001`, `val={21.5,ON}`, and `ep=1` to the exact stored command topic

## MODIFIED Requirements

### Requirement: Host subscribes to WiFi state topics
For each registered `mqtt` device with a non-empty state topic, while MQTT is connected the host SHALL subscribe to that state topic and, when channels use suffix mapping and `zigbeeProtocol` is false, the suffix wildcard needed for extra endpoints. Inbound payloads on those topics SHALL update that device’s runtime status using channels mapping, except when `zigbeeProtocol` is true the host SHALL use exact-topic and `ep=` rules instead.

#### Scenario: Subscribe single channel

- **WHEN** MQTT connects and an `mqtt` device has channels `1` and state topic `z2m/plug/state`
- **THEN** the host is subscribed to `z2m/plug/state`

#### Scenario: Suffix subscribe

- **WHEN** MQTT connects and an `mqtt` device has `zigbeeProtocol` false, channels `3`, and state topic `z2m/gang/state`
- **THEN** the host is subscribed to `z2m/gang/state` and `z2m/gang/state/+`

#### Scenario: No suffix subscribe when protocol on
- **WHEN** MQTT connects and an `mqtt` device has `zigbeeProtocol` true, channels `3`, and state topic `z2m/plug/state`
- **THEN** the host is subscribed to `z2m/plug/state` and is not subscribed to `z2m/plug/state/+`

### Requirement: Console manual command uses the set-topic path

A Manual command body for a registered `zigbee` device SHALL be applied with the same channels mapping, FULL CONTROL, ZCL cluster-command, and on/off rules as a payload received on that device’s MQTT command (`set`) topic. Suffix mode SHALL send the body to the chosen endpoint. Parse mode SHALL treat the chosen channel as `ch-<ep>##` wrapping. Single-channel SHALL send the body to the mapped endpoint as today. The host SHALL NOT require the MQTT broker to be connected for `zigbee` Manual command. Gateway topics (`bridge/permit_join`, `bridge/config/device`) SHALL NOT accept this path. For a registered `mqtt` device with `zigbeeProtocol` false, Manual command SHALL publish the body to that device’s MQTT command topic with channels mapping and SHALL NOT send Zigbee radio commands. For a registered `mqtt` device with `zigbeeProtocol` true, Manual command SHALL publish a Zigbee-shaped body to the exact stored command topic (encoding shortcuts and including `ep` from the chosen channel; `userData` uses `cl=0xFC00`) and SHALL NOT send Zigbee radio commands.

#### Scenario: Same as MQTT ON

- **WHEN** FULL CONTROL is off and Manual command sends `ON` for a `channels` `1` `zigbee` device
- **THEN** the host sends the on/off command with payload `ON` to the endpoint from channels mapping

#### Scenario: Parse mode channel

- **WHEN** channels is `0` and Manual command sends `OPEN` with channel 3 selected for a `zigbee` device
- **THEN** the host sends `OPEN` to endpoint 3 using the same ZCL covering command as MQTT

#### Scenario: Full control body

- **WHEN** FULL CONTROL is on and Manual command sends `cl=0x0006,attr=0x0000,val=0x1` for a `zigbee` device
- **THEN** the host sends that write-attribute the same way MQTT `set` would

#### Scenario: Covering stop from Command tab

- **WHEN** Manual command sends `STOP` for a `windowCovering` `zigbee` device
- **THEN** the host sends cluster `0x0102` command `0x02` the same way MQTT `set` would

#### Scenario: WiFi Manual publishes set

- **WHEN** Manual command sends `ON` for an `mqtt` device with `zigbeeProtocol` false, channels `1`, and MQTT is connected
- **THEN** the host publishes `ON` to that device’s command topic and does not send Zigbee on/off

#### Scenario: WiFi Zigbee-protocol Manual encodes

- **WHEN** Manual command sends `ON` for an `mqtt` device with `zigbeeProtocol` true, channel 2 selected, and MQTT is connected
- **THEN** the host publishes a Zigbee-shaped on body including `ep=2` to the exact stored command topic and does not send Zigbee radio commands
