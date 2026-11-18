# Spec Delta

## ADDED Requirements

### Requirement: Registered devices store router id
Each registered Zigbee device record SHALL persist a 4-byte `router_id` (`0` = local radio; non-zero = paired interconnect router). The field SHALL round-trip through `devices.json`, list JSON after reboot, single-device save/read, and export/import. Omitted on load SHALL mean `0`. The slave SHALL NOT persist or interpret `router_id`.

#### Scenario: Local default
- **WHEN** the operator saves a new Zigbee device found on the local radio without a router id
- **THEN** the stored `router_id` is 0

#### Scenario: Remote save keeps router id
- **WHEN** the operator saves a found device that arrived from a paired Repeater with router id 7
- **THEN** the registered record stores `router_id` 7 and still shows 7 after reboot

#### Scenario: List JSON includes router id
- **WHEN** the host lists registered devices
- **THEN** each Zigbee row includes its stored `router_id`

#### Scenario: MQTT defaults to zero
- **WHEN** the operator saves an MQTT-transport device without a router id
- **THEN** the stored `router_id` is 0

### Requirement: Found devices carry source router id
While search is active, each found (not yet registered) Zigbee entry SHALL include the source `router_id` (`0` for local Main radio, non-zero for a Repeater). Registering that found entry SHALL copy its `router_id` into the new registered record.

#### Scenario: Found from repeater
- **WHEN** search is active and a Repeater reports a new IEEE to Main
- **THEN** the found entry’s `router_id` is that Repeater’s id
