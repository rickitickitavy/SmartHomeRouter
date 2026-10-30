# Spec Delta

## ADDED Requirements

### Requirement: Registered Zigbee devices store last-known NWK short
Each registered Zigbee device record SHALL store a last-known NWK short address (16-bit). Missing or unknown SHALL be stored as `0`. The field SHALL appear in `devices.json` and list JSON after reboot (for example as `nwk`). MQTT-transport records MAY omit a usable short. The slave SHALL NOT be the durable store for this field.

#### Scenario: Short survives host reload
- **WHEN** a Zigbee device was last seen on short `0xA369` and the host reloads the device store
- **THEN** that IEEE’s registered record still has last-known short `0xA369`

#### Scenario: Unknown default
- **WHEN** the operator saves a new Zigbee device that has never reported a short
- **THEN** that record’s last-known short is `0`

### Requirement: Host updates last-known short from radio identity
When the host receives a slave join, identity, or attribute/zone report for a registered Zigbee IEEE with a usable short (non-zero and not `0xFFFF`), the host SHALL set that IEEE’s stored last-known short to that value. If another registered Zigbee IEEE still stores the same short, the host SHALL clear that other record’s last-known short to `0`. The host SHALL persist the change with the device store.

#### Scenario: Report refreshes short
- **WHEN** a registered IEEE reports with short `0xDBBC`
- **THEN** that IEEE’s stored last-known short becomes `0xDBBC`

#### Scenario: Short moves to another IEEE
- **WHEN** IEEE A stored short `0xDBBC` and IEEE B later reports with short `0xDBBC`
- **THEN** IEEE B stores `0xDBBC` and IEEE A’s last-known short becomes `0`
