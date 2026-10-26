# Proposal

## Why

Under command bursts (many devices / endpoints), the slave deferred outgoing Zigbee device-control table fills at **32** slots and drops new distinct destinations (`Deferred device command table full`). Raising that capacity to **128** lets larger bursts wait for the radio without changing the one-in-flight-per-IEEE+endpoint rule.

## What Changes

- Raise the slave deferred outgoing Zigbee device-command table from **32** to **128** (`InterChipSlave::kMaxDeferredDeviceCommands`).
- Keep existing semantics: latest command per IEEE+endpoint replaces an earlier deferred slot; in-flight gating unchanged.
- Do **not** change the SPI outbound-to-host queue (`kQueue`) or the host SPI out queue unless separately requested.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `zigbee-slave-radio`: deferred outgoing device-control table MUST hold at least 128 distinct IEEE+endpoint slots.

## Impact

- Slave: `include/InterChipSlave.h` (`kMaxDeferredDeviceCommands` 32 → 128); heap use grows when many deferred slots are allocated (`calloc` per new destination).
- `FIRMWARE_VERSION` propose bump `3.5.67` → `3.5.68` (apply bumps again when source lands).
