# Proposal

## Why

After the coordinator (slave) reboots—common during firmware updates—sleepy Zigbee devices keep reporting on their NWK short address, but the stack no longer maps that short to IEEE. The slave then forwards attribute reports with an all-zero IEEE; the host logs “Unmapped device state” and never updates state or availability. Registered sensors look offline even though RF traffic continues, which makes the router useless until devices re-identify or are re-paired.

## What Changes

- Persist each registered Zigbee device’s **last-known NWK short address** on the host with the device record (`devices.json` / device store).
- Carry that short address on host→slave device sync (`SpiCmdSetDevice`) so the slave can seed its short↔IEEE bound map as soon as the registry is applied after radio start.
- On the slave, when a report arrives with short address only (or stack IEEE lookup fails), resolve IEEE from the seeded last-known short map before forwarding to the host—for **all** device kinds that use the same report path.
- When the slave (or host) learns a usable short for a registered IEEE (join, identity, successful resolve, or inbound report with both), **update** the stored last-known short and clear any other registered device that still claimed that short.
- Propose bump: `FIRMWARE_VERSION` `3.9.20` → `3.9.21` (apply bumps again when source lands).

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `device-registry`: Registered Zigbee devices store and reload last-known NWK short; host updates it from identity/report events.
- `host-slave-spi`: Device sync entry includes last-known short; attribute reports continue to carry IEEE+short (IEEE must be resolved on the slave when possible).
- `zigbee-slave-radio`: After registry sync, seed short↔IEEE; resolve inbound reports via last-known short when the stack table misses.

## Impact

- Host: `DeviceTopicEntry` / `DeviceTopicMap` persist + JSON; SPI pack/unpack length; update short on mapped reports/joins.
- Slave: `ZigbeeCoordinator` upsert/registry apply seeds `rememberShortIeee`; `resolveIeeeFromSource` uses that map for multi-device orphans (today `fillIeeeFromUniqueUnresolved` only helps when exactly one device lacks a short).
- No operator UI required for the first fix; Devices JSON may gain an `nwk` field.
- Does not change Zigbee channel, pan id, or require re-pair when the last-known short is still valid on air.
