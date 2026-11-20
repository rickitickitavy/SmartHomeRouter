## ADDED Requirements

### Requirement: Orphan device router_id can be reassigned from Main
On Main, when a registered Zigbee device’s `router_id` refers to a Repeater that is no longer paired, the operator MUST be able to change that device’s `router_id` to **local** (`0`) or to a currently paired Repeater’s id through the device dialog Save path, without deleting the device. After Save, Zigbee command routing MUST use the new `router_id` (local radio for `0`, or the matching peer for a non-zero id) under existing interconnect command rules.

#### Scenario: Reassign orphan to local
- **WHEN** a registered device has `router_id` 4, peer 4 is unpaired, and the operator Saves with local selected
- **THEN** the device’s stored `router_id` is 0 and later commands use the local radio path

#### Scenario: Reassign orphan to another paired Repeater
- **WHEN** a registered device is orphaned from `router_id` 4 and the operator Saves with paired Repeater id 2 selected
- **THEN** the device’s stored `router_id` is 2 and later commands target peer 2 when that peer is online
