# Spec Delta: router-interconnect

## ADDED Requirements

### Requirement: Main pushes time zone offset to Repeaters
When network role is Main and the operator saves a UTC time-zone offset (minutes from UTC, 30-minute steps), the host SHALL push that offset to all paired Repeaters via the interconnect variable exchange (same Main-owned push path as other shared settings). A Repeater SHALL apply the received offset to its local wall-clock / NTP configuration and SHALL NOT treat a conflicting local Hardware edit as authoritative.

#### Scenario: Save on Main updates paired Repeaters
- **WHEN** Main saves timeZoneUtcOffsetMin 180 with two online paired Repeaters
- **THEN** both Repeaters receive and apply offset +180 minutes

#### Scenario: Boot hello refreshes offset
- **WHEN** a paired Repeater completes just_booted with Main
- **THEN** Main includes the current UTC offset in the shared variable push to that peer

### Requirement: Status API exposes aggregated repeater packet totals
On Main, the gateway status (or interconnect status slice used by Status) SHALL expose aggregated **Repeater packets sent** and **Repeater packets received** as the sum of paired peers’ interconnect packets Out and packets In respectively. When role is Repeater or there are no peers, both aggregates SHALL be 0.

#### Scenario: Aggregate from peers
- **WHEN** Main has peers with packetsOut 10 and 5 and packetsIn 3 and 2
- **THEN** status reports repeater packets sent 15 and received 5 for Status Network
