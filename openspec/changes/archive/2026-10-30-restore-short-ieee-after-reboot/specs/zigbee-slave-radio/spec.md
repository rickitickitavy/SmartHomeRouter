# Spec Delta

## ADDED Requirements

### Requirement: Seed short↔IEEE from synced last-known short
When the slave applies a registered Zigbee device sync entry that includes a usable last-known NWK short (non-zero and not `0xFFFF`), the slave SHALL associate that short with that IEEE in its runtime short↔IEEE map before or as it queues boot status reads. This SHALL apply to every registered Zigbee device kind using the same registry sync path. The slave SHALL NOT require the Zigbee stack neighbor table to already contain that mapping.

#### Scenario: After reboot with stored short
- **WHEN** the host syncs a registered sensor IEEE with last-known short `0xDBBC` after coordinator start
- **THEN** the slave can resolve short `0xDBBC` to that IEEE without a new join

#### Scenario: Unknown short skipped
- **WHEN** a sync entry has last-known short `0`
- **THEN** the slave does not invent a short mapping for that IEEE from the sync alone

### Requirement: Resolve report IEEE from last-known short map
When a Zigbee attribute or zone report arrives with a short source and the stack has no usable IEEE for that short, the slave SHALL resolve IEEE from its runtime short↔IEEE map (including last-known seeds) before forwarding. On success it SHALL send that IEEE with the short; on failure it MAY send an all-zero IEEE. This applies to every device kind on the shared report path.

#### Scenario: Sleepy report after coordinator reboot
- **WHEN** a registered device reports humidity on short `0xDBBC` after a coordinator reboot and the stack IEEE table misses that short, but the slave has last-known short `0xDBBC` for that IEEE
- **THEN** the host receives the report with that IEEE (not all zeros) and can map it to the registered device

#### Scenario: Truly unknown short stays unmapped
- **WHEN** a report arrives on a short that is not in the slave short↔IEEE map and the stack has no IEEE
- **THEN** the forwarded report has an all-zero IEEE
