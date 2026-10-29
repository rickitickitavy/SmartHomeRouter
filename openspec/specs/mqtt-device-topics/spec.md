# mqtt-device-topics Specification

## Purpose

Maps each registered device’s stored MQTT topics using a per-device channels setting: single-channel as today, suffix topics for extra endpoints, or one topic with a `ch-<ep>##` payload prefix.

## Requirements

### Requirement: Channels setting selects topic mapping
Each registered device SHALL store a channels value of `0` (multichannel with parsing), or `1` through `16`. The default SHALL be `1`. Value `1` SHALL use the stored state and command topics with no endpoint suffix. Values `2` through `16` SHALL use suffix mapping. Value `0` SHALL use parse mapping. Availability SHALL always use the stored availability topic with no suffix and no payload prefix.

#### Scenario: Default single channel
- **WHEN** a device is saved without a channels value
- **THEN** channels is `1` and state publishes go to the stored state topic

#### Scenario: Availability unchanged
- **WHEN** a registered device has stored availability `z2m/switcher_1/availability`
- **THEN** availability traffic uses that topic exactly

### Requirement: Suffix mode uses endpoint in the topic except ep 1
When channels is `2` through `16` and the host publishes or receives device traffic, endpoint `1` SHALL use the stored topic with no suffix. A usable endpoint other than `1` SHALL use `{storedTopic}/{ep}`. Endpoint `255` SHALL NOT be used as a topic suffix. The payload body SHALL be the device message as received or as published, not restricted to `ON` or `OFF`.

#### Scenario: Gang 3 in suffix mode
- **WHEN** a device with channels `4` and stored state `z2m/switcher_1/state` reports `ON` from endpoint 3
- **THEN** the host publishes `ON` to `z2m/switcher_1/state/3`

#### Scenario: Gang 1 stays unsuffixed
- **WHEN** that same device reports `OFF` from endpoint 1
- **THEN** the host publishes `OFF` to `z2m/switcher_1/state`

#### Scenario: Command suffix
- **WHEN** MQTT receives any payload on `z2m/switcher_1/set/2` for that device
- **THEN** the host sends that payload to IEEE endpoint 2

### Requirement: Parse mode prefixes the payload with the endpoint
When channels is `0`, the host SHALL publish and subscribe on the stored state and command topics with no suffix. An inbound device message SHALL be published as `ch-<ep>##` followed by the original message. An outbound MQTT payload `ch-<ep>##<message>` SHALL send `<message>` to that endpoint. The host SHALL NOT require the message body to be `ON` or `OFF`.

#### Scenario: Parse publish
- **WHEN** a parse-mode device reports `LEAK` from endpoint 3
- **THEN** the host publishes `ch-3##LEAK` to the stored state topic

#### Scenario: Parse command
- **WHEN** MQTT receives `ch-3##OPEN` on the stored command topic
- **THEN** the host sends `OPEN` to endpoint 3

### Requirement: Full control parses command payload into write fields
When a registered device has FULL CONTROL on, the host SHALL apply channels mapping first, then parse the remaining command body as comma-separated `key=value` pairs. Recognized keys SHALL include `cl` (cluster id), `attr` (attribute id), `val` (value), `ep` (destination endpoint), and `type` (ZCL attribute data type). Values SHALL accept decimal or `0x` hexadecimal. Unknown keys SHALL be ignored. A key is parsed only when it appears as `key=value`. When at least one recognized key is parsed, the host SHALL send a Zigbee write-attribute using parsed values and defaults for every recognized attribute that was absent: `cl` `0x0006`, `attr` `0x0000`, `val` `0`, `ep` the endpoint from channels mapping, `type` unsigned 8-bit unless `val` needs a wider unsigned type.

#### Scenario: Report-shaped payload
- **WHEN** FULL CONTROL is on and MQTT receives `cl=0x0006,attr=0x0000,val=0x1` on that device’s command topic
- **THEN** the host sends a write-attribute for cluster `0x0006`, attribute `0x0000`, value `1` to the endpoint from channels mapping

#### Scenario: Absent attributes use defaults
- **WHEN** FULL CONTROL is on and MQTT receives `cl=0x0102,val=0x1`
- **THEN** the host sends a write-attribute for cluster `0x0102`, attribute `0x0000`, value `1`, type unsigned 8-bit, to the endpoint from channels mapping

#### Scenario: Endpoint override in payload
- **WHEN** FULL CONTROL is on, channels mapping chose endpoint 1, and the body is `cl=0x0006,attr=0x0000,val=0,ep=3`
- **THEN** the host sends the write to endpoint 3

#### Scenario: Parse mode then full control
- **WHEN** channels is `0`, FULL CONTROL is on, and MQTT receives `ch-2##cl=0x0006,attr=0x0000,val=1`
- **THEN** the host writes cluster `0x0006` attribute `0x0000` value `1` to endpoint 2

### Requirement: Full control unparsed body is the command payload
When FULL CONTROL is on and the remaining command body contains no parsed recognized attributes, the host SHALL send that entire body as the command payload (same on/off/toggle text path as today) and SHALL apply defaults for the other attributes (`ep` from channels mapping).

#### Scenario: Plain ON with full control
- **WHEN** FULL CONTROL is on and MQTT receives `ON` on that device’s command topic
- **THEN** the host sends the on/off command with payload `ON` to the endpoint from channels mapping

#### Scenario: Parse mode plain body
- **WHEN** channels is `0`, FULL CONTROL is on, and MQTT receives `ch-3##OFF`
- **THEN** the host sends payload `OFF` to endpoint 3

### Requirement: Full control off keeps on/off commands
When FULL CONTROL is off or unset, the host SHALL keep existing command behavior: channels mapping, then on/off/toggle text to the device.

#### Scenario: Default device still toggles
- **WHEN** a device was saved without FULL CONTROL and MQTT receives `toggle` on its command topic
- **THEN** the host sends the on/off toggle command as today

### Requirement: Command topic sends ZCL cluster commands
After channels mapping, a body on a registered device’s MQTT command (`set`) topic SHALL be applied in this order: FULL CONTROL write-attribute when that parse succeeds; else a ZCL cluster command when the body is a covering shortcut or contains `cl` and `cmd`; else today’s on/off/toggle text. Shortcuts SHALL be case-insensitive: `OPEN` and `UP` send cluster `0x0102` command `0x01`; `CLOSE` and `DOWN` send `0x0102` command `0x00`; `STOP` sends `0x0102` command `0x02`. Generic bodies SHALL accept `cl` (cluster id) and `cmd` (ZCL command id) as decimal or `0x` hex, optional `payload` bytes, and SHALL work for every registered device type. Suffix and parse channel mapping SHALL stay as they are, including suffix on the first channel when channels is `2`–`16`. Gateway topics SHALL NOT accept this path.

#### Scenario: Covering open
- **WHEN** MQTT receives `OPEN` on a `windowCovering` device’s command topic with channels `1`
- **THEN** the host sends a ZCL cluster command for cluster `0x0102` command `0x01` to the mapped endpoint

#### Scenario: Generic command any type
- **WHEN** MQTT receives `cl=0x0102,cmd=0x02` on an `onOff` device’s command topic
- **THEN** the host sends cluster `0x0102` command `0x02` to the mapped endpoint

#### Scenario: Suffix first channel
- **WHEN** a device has channels `2` and MQTT receives `STOP` on `{commandTopic}/1`
- **THEN** the host sends cluster `0x0102` command `0x02` to endpoint 1

#### Scenario: On/off unchanged
- **WHEN** FULL CONTROL is off and MQTT receives `ON` on a `channels` `1` device
- **THEN** the host still sends the on/off command with payload `ON`

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

### Requirement: MQTT settings store server-born topic and interval
The MQTT settings group SHALL store a server-born topic string and a born announce interval in minutes. The interval SHALL clamp to 5–120 inclusive; the factory default SHALL be 20. The server-born topic MAY be empty. Empty topic SHALL disable born listen and born publish. Settings GET/POST, export, and restore SHALL include both fields.

#### Scenario: Default interval
- **WHEN** MQTT settings are initialized to defaults
- **THEN** born announce interval is 20 and server-born topic is empty

#### Scenario: Clamp interval
- **WHEN** the operator saves born interval 2
- **THEN** the stored interval is 5

#### Scenario: Empty topic disables born
- **WHEN** server-born topic is empty and MQTT is connected
- **THEN** the host does not subscribe for born and does not publish born announces

### Requirement: Server-born listen refreshes Zigbee states

When the server-born topic is non-empty and MQTT is connected, the host SHALL subscribe to that exact topic. On receipt of any payload on that topic, the host SHALL collect states of all registered `zigbee` devices the same way it does at boot (status reads that lead to state publishes when replies arrive). The host SHALL NOT require a particular payload body. The host MUST NOT run that full Zigbee status-collect pass nested inside the same call that performs the entire device-topic subscribe/unsubscribe burst for session attach; status collect MAY start on a later service-loop iteration after born is handled.

#### Scenario: Any born message

- **WHEN** MQTT delivers any message on the configured server-born topic
- **THEN** the host starts a Zigbee status collect for registered Zigbee devices

#### Scenario: Born does not stack on subscribe burst

- **WHEN** a server-born message is handled while remote session attach subscriptions are still being installed or have just finished in a burst
- **THEN** Zigbee status collect still occurs, but not as a nested call that deepens the same stack frame as the full subscribe walk

#### Scenario: WiFi excluded from radio collect

- **WHEN** a born message arrives and the map has both `zigbee` and `mqtt` devices
- **THEN** only `zigbee` devices are included in the Zigbee status collect

### Requirement: Host announces on server-born topic after boot and on interval

When the server-born topic is non-empty and MQTT is connected, the host SHALL publish a message to that topic once at least 2 minutes after host boot, then again every configured born announce interval. The publish SHALL use a non-empty payload. The purpose is to tell MQTT devices (and other listeners) to refresh their statuses.

#### Scenario: First announce after boot

- **WHEN** MQTT is connected, server-born topic is set, and 2 minutes have passed since host boot without a prior host born publish in this run
- **THEN** the host publishes once to the server-born topic

#### Scenario: Periodic announce

- **WHEN** the configured interval is 20 and the host already published a born announce
- **THEN** the host publishes again about 20 minutes later while MQTT stays connected

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

### Requirement: Zigbee On/Off state publishes both ON and OFF
When the host receives a Zigbee-sourced On/Off state report for a registered device and MQTT is connected, it SHALL publish that report on the mapped state topic with payload `ON` or `OFF` matching the report. A rapid sequence that includes both values SHALL produce a publish for each accepted report, including `OFF` after `ON` within a few seconds.

#### Scenario: OFF after rapid toggle
- **WHEN** a registered Zigbee `onOff` device with channels `1` reports `ON` then `OFF` on endpoint 1 and MQTT is connected
- **THEN** the host publishes `ON` then `OFF` on that device’s stored state topic

#### Scenario: Suffix mode OFF after ON
- **WHEN** a registered Zigbee device with channels `4` reports `ON` then `OFF` from endpoint 3 and MQTT is connected
- **THEN** the host publishes `ON` then `OFF` on `{storedStateTopic}/3`

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

### Requirement: Remote MQTT session attach paces device subscriptions

When the host finishes attaching a remote MQTT session (after a successful TCP/MQTT connect), it MUST establish the same set of topic subscriptions required by existing born / MQTT-device / Zigbee `publishToMqtt` rules, but MUST NOT perform the entire device-map subscribe/unsubscribe pass as one uninterrupted burst that can overflow host task stacks or reboot the chip. The host MAY spread subscribe and unsubscribe work across successive service-loop iterations. Within a short settle window after connect (on the order of a few seconds for a typical device map), the subscribed set MUST match today’s rules. The host MUST remain responsive enough that MQTT client pump and host SPI keepalive can progress while attach subscriptions are still completing.

#### Scenario: Large map after remote connect

- **WHEN** remote MQTT connects while the host has dozens of registered devices that each require state and/or command topic subscriptions
- **THEN** the host completes those subscriptions without a stack-canary panic or reboot, and the final subscribed set includes the topics those devices require under existing rules

#### Scenario: Attach does not block keepalive forever

- **WHEN** remote MQTT session attach is still installing device topic subscriptions
- **THEN** the host service loop continues to run within a few seconds between subscription slices (SPI keepalive and MQTT client progress are not frozen for tens of seconds)

### Requirement: User-data MQTT uses brace-list payload

MQTT traffic for `userData` devices MUST use a `{…}` brace list matching `userDataFields` order, without a `cl=`/`attr=` wrapper. Null slots are `NULL` or empty (`,,`). Bool accepts `true`/`false` and `on`/`off`. Internally the list maps to cluster `0xFC00` attr `0x0001`. Outbound state publish MUST use the same brace-list form.

#### Scenario: MQTT state brace list

- **WHEN** an MQTT `userData` device with three schema fields receives state `{1000,,ON}`
- **THEN** the host maps slot 0 and slot 2 to the first and third fields and treats the middle field as null/empty

#### Scenario: Publish brace list only

- **WHEN** the host publishes MQTT state for a `userData` device
- **THEN** the body is a `{…}` list without a leading `cl=0xFC00` prefix

### Requirement: Custom command execute uses transport command path
When the host executes a registered device custom command, it MUST use the same channels mapping as other outbound commands for that device. For `mqtt` transport it MUST publish the custom-command payload to the mapped command topic and MUST NOT send Zigbee frames for that execute. For `zigbee` transport it MUST apply the custom-command body on the Zigbee device-command path.

#### Scenario: MQTT custom command publishes only
- **WHEN** the host executes custom command `reboot` on an `mqtt` device
- **THEN** the host publishes `reboot` on that device’s mapped command topic and does not send Zigbee on/off or ZCL frames for that execute

#### Scenario: Zigbee custom command uses body path
- **WHEN** the host executes custom command `scene` with value `hall` on a `zigbee` device
- **THEN** the host applies body `cmd=scene,val=hall` through the Zigbee device-command path for that device’s mapped channel
