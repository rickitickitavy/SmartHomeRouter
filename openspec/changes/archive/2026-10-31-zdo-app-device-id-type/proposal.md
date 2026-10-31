# Proposal

## Why

Device class today is driven largely by in/out-cluster guesses, which races and mislabels products (for example On/Off gear shown as measurement types). Zigbee already exposes a standard Application Device ID on the Simple Descriptor (ZDO Simple Descriptor Request / response). Manufacturer and model belong to product-specific Configure catalogs, not to rewriting device class—especially not on devices already saved in the registry.

## What Changes

- **Classify new Zigbee device type from Application Device ID only** (ZDO Simple Descriptor `app_device_id`), using the HA/ZHA (+ ZLO) Device ID table. **Do not** use in/out clusters to assign device type.
- **Never change type for already paired/registered devices** (host store and slave apply-saved-class). Type updates from discovery apply only to found / not-yet-saved devices.
- **Manufacturer Name + Model Identifier** (Basic `0x0004` / `0x0005`) continue to be read as today and are used to select **configurable attributes** in the Configure catalog—not to set or override device type.
- Expand HA Application Device ID → internal type mapping to cover the standard HA/ZHA/ZLO IDs needed for correct class (On/Off light/plug, dimmable/color, covering, thermostat, temperature sensor, IAS zone, etc.).
- **BREAKING** relative to current slave behavior: cluster-priority classification (`0x0500` / `0x0102` / `0x0006`) no longer defines type for new joins.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `zigbee-slave-radio`: Replace in-cluster type classification with Application Device ID from Simple Descriptor; forbid cluster-list and manuf/model from setting type.
- `device-registry`: Found/new devices take type from that ZDO classification; registered Zigbee devices keep stored type forever after Save (no later join/identity rewrite).
- `device-configure`: Clarify that manuf + model (+ type) select configurable attributes only; they MUST NOT redefine device type.

## Impact

- Slave: `ZigbeeCoordinator::onSimpleDescriptor` and `zigbeeDeviceTypeFromHaDeviceId` / related helpers; stop merging cluster classification into type.
- Host: `FoundDeviceList` / registry persistence must not overwrite a saved Zigbee type from later joins.
- Configure UI/catalog already keys on manufacturer, type, optional model—keep that split.
- Firmware version build bumped for this propose (`3.9.29`).
