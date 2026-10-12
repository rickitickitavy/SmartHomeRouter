# Proposal

## Why

Automation Trigger, Condition, and Actions still expose raw cluster numbers (or free-text payloads) instead of the device’s known status parameters. Operators need to listen for and act on specific interpreted events (Status, Position, Battery, Light level, …) with a clear Custom escape hatch—without oversized Remove controls or always-visible channel pickers.

## What Changes

- **Trigger (listening devices):** each row picks device, channel (hidden when the device has only one channel), and a **known parameter** for that device type **or CUSTOM**. CUSTOM shows cluster and attrId. Listening MUST match that specific event/attribute, not “any change on the device.”
- **Condition comparisons:** the same device / channel / known-parameter-or-CUSTOM picker. Known parameters compare the host’s interpreted status strings (e.g. ON/OFF, STOPPED). CUSTOM compares the numeric raw attribute value (**VAL**) with the condition’s compare ops.
- **Device actions:** the same picker pattern. Known parameters map to the existing device-command payload vocabulary. CUSTOM shows cluster, command, and value fields and sends that raw command.
- **UI polish:** Trigger **Remove** buttons match the size/style of Remove controls in Condition/Actions (`btn-inline` / shared toolbar sizing).
- **Known parameter catalog (initial):** Window covering (Status: STOPPED / MOVING UP / MOVING DOWN; Position); On/Off (Status ON/OFF); Level/dimmer (On/Off status + Light level); sensors (Measured value / Sensor value; Battery level when available); IAS zone (alarm/dry-style status); optional occupancy and similar measurement clusters already classified in firmware.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: Automation edit Trigger / Condition / Actions pickers, channel visibility, Remove button sizing, known-parameter lists.
- `automations`: Persist and evaluate typed listen/condition/action parameter bindings (known key vs custom cluster/attr or command); stricter listen matching; custom condition VAL compares.

## Impact

- Automation JSON shape gains typed parameter fields on listeners, condition comparisons, and device actions; older automations without those fields keep working with documented defaults.
- Host evaluation and (if needed) device-command path must honor attribute-specific listen and custom command/value actions.
- Web console automation editor UI and CSS only for Remove sizing; firmware build bumps with the change.
