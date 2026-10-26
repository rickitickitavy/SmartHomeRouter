# Spec Delta

## ADDED Requirements

### Requirement: Deferred outgoing device-command table holds 128 slots
The slave MUST retain a deferred outgoing Zigbee device-control table of at least **128** distinct IEEE and destination-endpoint slots. Newer commands for an existing IEEE+endpoint MUST replace that slot. A new destination MUST NOT be allocated when the table is already full. One-in-flight-per-destination radio rules are unchanged.

#### Scenario: One hundred twenty-eight distinct destinations deferred
- **WHEN** the host delivers deferred device-control commands for 128 different IEEE+endpoint pairs before the radio has drained them
- **THEN** the slave retains all 128 deferred destinations without rejecting the 128th for capacity

#### Scenario: One hundred twenty-ninth distinct destination rejected
- **WHEN** the deferred table already holds 128 distinct IEEE+endpoint destinations and a device-control command arrives for a new IEEE+endpoint
- **THEN** the slave does not allocate a 129th slot and does not put that new destination’s command into the deferred table

#### Scenario: Same destination still replaces
- **WHEN** the deferred table is full at 128 destinations and a newer on/off arrives for an IEEE+endpoint that already has a deferred slot
- **THEN** that slot’s deferred payload is replaced with the newer command
