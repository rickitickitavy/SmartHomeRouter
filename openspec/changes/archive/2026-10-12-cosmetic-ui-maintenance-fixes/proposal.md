# Proposal

## Why

Several console and runtime behaviors are awkward for day-to-day operators: firmware update progress lives in a modal with outdated labels, a second update can be started while one is already running, Maintenance has no reboot control, login does not focus the username field, MQTT/ZigBee saves force a full router restart, and the update LED blinks faster than needed.

## What Changes

- Rename the firmware progress phase label **Master to slave** to **RF firmware update**.
- Move all firmware-update progress, status, reboot-wait, and close behavior from the modal dialog into the System → Update card (remove the modal as the operator surface).
- When the operator opens the Update card, query current update status; if an update is already active, disable controls that start a new update.
- Add a **REBOOT** control on System → Maintenance that requires two sequential approvals before rebooting the router.
- On the login form, set keyboard focus to the username field by default when the form is shown.
- Saving MQTT settings must persist and apply without restarting the router; same for ZigBee settings.
- Slow host LED6 firmware-update blink from 10 Hz to 5 Hz.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: Update card owns firmware progress UI; active-update guard; Maintenance reboot with double confirm; login username focus; MQTT/ZigBee save responses and apply-without-restart behavior; Update-dialog wording/result-plate requirements retargeted to the Update card.
- `status-rgb-led`: Host LED6 firmware-update blink rate 10 Hz → 5 Hz.
- `mqtt-broker`: SERVER TYPE / broker apply after MQTT save no longer depends on a router restart.
- `zigbee-slave-radio`: ZigBee channel / permit-join settings apply after save without a host/router restart (slave radio may re-apply).

## Impact

- Web UI: `data/index.html` (and login focus in `data/user.html` if shared), related CSS if needed.
- Host HTTP: MQTT/ZigBee POST handlers (`WebConsole`), new admin reboot API, firmware status already used by Update UI.
- Runtime apply: MQTT client/broker hot-apply; ZigBee settings push to slave and slave apply when coordinator is already running.
- Status LEDs: `StatusRgb` update blink half-period (`kUpdateBlinkHalfMs` 50 → 100).
- Specs above; firmware build bumped to `3.3.25` during propose.
- Fragile note: live ZigBee re-push may touch `InterChipHost` / `InterChipSlave` (influence of `slave-spi-ota`). Apply must not edit hard-frozen OTA paths; influence edits need explicit zone approval before implementation.
