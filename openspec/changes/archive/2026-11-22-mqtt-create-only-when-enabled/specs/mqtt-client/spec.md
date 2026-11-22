## Purpose

Host MQTT client object is allocated only when MQTT is enabled (remote or local).

## ADDED Requirements

### Requirement: Host MQTT client exists only when enabled
The host SHALL construct the MQTT client (including its network client) only when stored SERVER TYPE is `remote` or `local`. When SERVER TYPE is `disable`, the host SHALL NOT keep an MQTT client instance. Changing SERVER TYPE to `disable` after an MQTT save SHALL destroy the client without requiring a router restart. Changing from `disable` to `remote` or `local` after an MQTT save SHALL create and start the client without requiring a router restart.

#### Scenario: Boot with disable
- **WHEN** the host boots with SERVER TYPE `disable`
- **THEN** no host MQTT client instance exists and Status shows MQTT as not connected / disabled

#### Scenario: Enable remote without reboot
- **WHEN** an admin saves MQTT settings changing SERVER TYPE from `disable` to `remote`
- **THEN** the host creates the MQTT client and may connect to the remote SERVER without a router restart

#### Scenario: Disable destroys client
- **WHEN** an admin saves MQTT settings changing SERVER TYPE from `remote` or `local` to `disable`
- **THEN** the host destroys the MQTT client and Status no longer treats MQTT as connected
