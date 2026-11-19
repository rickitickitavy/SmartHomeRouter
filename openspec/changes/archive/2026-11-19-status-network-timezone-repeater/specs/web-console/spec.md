# Spec Delta: web-console

## ADDED Requirements

### Requirement: Hardware sets UTC time zone offset
System → Hardware SHALL show **Time zone** as a UTC-offset select in 30-minute steps (UTC−12:00…UTC+14:00). Persist minutes from UTC, round-trip on `/api/hardware`, export/restore with settings, and apply to NTP. Only Main `isAdmin` may edit.

#### Scenario: Default offset
- **WHEN** an admin opens Hardware and time zone was never saved
- **THEN** the control shows UTC+00:00 (0 minutes) or the documented factory default

#### Scenario: Save half-hour offset
- **WHEN** the admin selects UTC+05:30 and saves Hardware
- **THEN** GET `/api/hardware` returns timeZoneUtcOffsetMin 330 and the host applies that offset

#### Scenario: List uses 30-minute steps
- **WHEN** the admin opens the Time zone select
- **THEN** consecutive options differ by 30 minutes and no free-text entry is offered

### Requirement: Repeater time zone is Main-owned
When network role is Repeater, Hardware MUST NOT offer an editable Time zone control. The Repeater SHALL apply the UTC offset pushed from Main over interconnect. Hardware MAY show the effective offset as read-only.

#### Scenario: Repeater cannot edit time zone
- **WHEN** network role is Repeater and an admin opens Hardware
- **THEN** Time zone is absent or read-only and POST cannot change a local independent offset

#### Scenario: Main push updates Repeater
- **WHEN** Main saves a new UTC offset and the Repeater is paired
- **THEN** the Repeater applies that offset without a local Hardware save

### Requirement: Status Network shows repeater packet totals
Status **Network** SHALL add **Repeater packets sent** and **Repeater packets received**. On Main these are sums of peer packets Out / In; otherwise 0. Both Status pages MUST match. Values come from the gateway status API.

#### Scenario: Summed peer counters on Main
- **WHEN** Status is open on Main with two paired Repeaters whose packets Out are 10 and 5 and packets In are 3 and 2
- **THEN** Network shows Repeater packets sent 15 and Repeater packets received 5

#### Scenario: No peers
- **WHEN** Status is open on Main with no paired Repeaters
- **THEN** Network shows Repeater packets sent 0 and Repeater packets received 0

## MODIFIED Requirements

### Requirement: Status Network group
The Network group SHALL show Zigbee packets sent, MQTT packets sent, Zigbee packets received, MQTT packets received, **Repeater packets sent**, and **Repeater packets received**. Values SHALL come from the gateway status API and SHALL NOT mix Zigbee SPI traffic with MQTT application traffic in a single counter. Repeater packet totals SHALL be interconnect peer aggregates as specified for Status Network repeater packet totals.

#### Scenario: Separate packet counters
- **WHEN** the host has sent 4 Zigbee packets and 2 MQTT packets and received 9 Zigbee packets and 3 MQTT packets
- **THEN** Network shows Zigbee packets sent 4, MQTT packets sent 2, Zigbee packets received 9, and MQTT packets received 3

#### Scenario: Repeater totals beside Zigbee MQTT
- **WHEN** Status Network is open on Main with aggregated repeater sent 7 and received 4
- **THEN** Network shows those repeater totals in addition to the Zigbee and MQTT rows

### Requirement: Repeater role hides blocked console sections
When network role is Repeater, the console MUST hide sidebar sections Place, Devices, Automation, Cameras, Scenes, Consoles, and Security. System → Maintenance MUST hide all Maintenance cards except **Reboot**. System → Hardware MUST show only SPI SPEED, RF update SPI speed, BLUE LED brightness, GREEN LED brightness, use queue for base device commands, and Low memory threshold (and their Save/Reset as applicable), plus a read-only Time zone display when time zone is shown. Other Hardware fields MUST be hidden on Repeater. System → **Internal DB** MUST be **hidden** on Repeater.

#### Scenario: Repeater hides Devices
- **WHEN** network role is Repeater and an admin is signed in
- **THEN** Devices is not shown in the sidebar

#### Scenario: Repeater keeps Reboot
- **WHEN** network role is Repeater and an admin opens System → Maintenance
- **THEN** Reboot remains available and export/restore settings are hidden

#### Scenario: Repeater Hardware allow-list
- **WHEN** network role is Repeater and an admin opens System → Hardware
- **THEN** SPI speed, RF update SPI speed, blue/green LED brightness, use queue for base device commands, and low memory threshold are visible and other Hardware groups are hidden

#### Scenario: Repeater keeps Internal DB
- **WHEN** network role is Repeater and an admin opens System
- **THEN** the Internal DB tab is not shown
