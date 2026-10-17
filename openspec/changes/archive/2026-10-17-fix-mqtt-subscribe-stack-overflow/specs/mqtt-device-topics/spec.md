# Spec Delta

## ADDED Requirements

### Requirement: Remote MQTT session attach paces device subscriptions

When the host finishes attaching a remote MQTT session (after a successful TCP/MQTT connect), it MUST establish the same set of topic subscriptions required by existing born / MQTT-device / Zigbee `publishToMqtt` rules, but MUST NOT perform the entire device-map subscribe/unsubscribe pass as one uninterrupted burst that can overflow host task stacks or reboot the chip. The host MAY spread subscribe and unsubscribe work across successive service-loop iterations. Within a short settle window after connect (on the order of a few seconds for a typical device map), the subscribed set MUST match today’s rules. The host MUST remain responsive enough that MQTT client pump and host SPI keepalive can progress while attach subscriptions are still completing.

#### Scenario: Large map after remote connect

- **WHEN** remote MQTT connects while the host has dozens of registered devices that each require state and/or command topic subscriptions
- **THEN** the host completes those subscriptions without a stack-canary panic or reboot, and the final subscribed set includes the topics those devices require under existing rules

#### Scenario: Attach does not block keepalive forever

- **WHEN** remote MQTT session attach is still installing device topic subscriptions
- **THEN** the host service loop continues to run within a few seconds between subscription slices (SPI keepalive and MQTT client progress are not frozen for tens of seconds)

## MODIFIED Requirements

### Requirement: Server-born listen refreshes Zigbee states

When the server-born topic is non-empty and MQTT is connected, the host SHALL subscribe to that exact topic. On receipt of any payload on that topic, the host SHALL collect states of all registered `zigbee` devices the same way it does at boot (status reads that lead to state publishes when replies arrive). The host SHALL NOT require a particular payload body. The host MUST NOT run that full Zigbee status-collect pass nested inside the same call that performs the entire device-topic subscribe/unsubscribe burst for session attach; status collect MAY start on a later service-loop iteration after born is handled.

#### Scenario: Any born message

- **WHEN** MQTT delivers any message on the configured server-born topic
- **THEN** the host starts a Zigbee status collect for registered Zigbee devices

#### Scenario: Born does not stack on subscribe burst

- **WHEN** a server-born message is handled while remote session attach subscriptions are still being installed or have just finished in a burst
- **THEN** Zigbee status collect still occurs, but not as a nested call that deepens the same stack frame as the full subscribe walk
