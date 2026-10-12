# Design

## Context

See `proposal.md` for motivation. Today firmware progress lives in `#firmwareUpdateDialog` while the Update card only has the file picker and Update button; MQTT/ZigBee `POST` handlers call `saveMain(true)` (host restart); slave `onSlaveSettings` ignores a second push once the coordinator is up; LED6 update blink uses `kUpdateBlinkHalfMs = 50` (10 Hz); login `showLoginForm` does not focus the username field; Maintenance has no reboot API.

Fragile zones: do **not** edit hard-frozen OTA files (`FirmwareOta`, joined ZIP scripts). Live ZigBee re-push may touch `InterChipHost` / `InterChipSlave` (influence of `slave-spi-ota`) — implement only after explicit approval naming that zone.

## Goals / Non-Goals

**Goals:**
- One Update-card surface for start, progress, reboot-wait, and active-update gating.
- MQTT and ZigBee console saves persist and apply without `ESP.restart` on the host.
- Maintenance admin reboot with two confirmations.
- Login username autofocus; LED6 update blink at 5 Hz.

**Non-Goals:**
- Changing joined OTA order, SPI OTA protocol, or ZIP layout.
- Changing Wi-Fi save/restart behavior or settings-restore restart.
- Redesigning Maintenance export/restore cards beyond adding REBOOT.
- Changing slave pairing LED or non-update LED meanings.

## Decisions

### 1. Inline Update card instead of modal

Move the phase list, status line, reboot-wait block, and close/clear control from `#firmwareUpdateDialog` into `#system-tab-update`’s upload card. Keep the same element IDs where practical so existing JS (`setFirmwarePhasePercent`, poll, reboot wait) keeps working with minimal retargeting. Remove the overlay (or leave it unused/hidden permanently). Escape/overlay dismiss paths that special-case `firmwareUpdateDialog` become no-ops or are deleted.

**Alternative considered:** Keep a thin modal only for reboot-wait — rejected; proposal requires all dialog content/functionality on the card.

### 2. Active-update guard on tab enter

In `showSystemTab('update')` (and first paint if Update is default — it is not), `GET /api/update/status`. Treat phases other than `idle` / terminal settled states as active (same notion the poll already uses: in-progress until `failed`/`idle`/done after reboot wait). Disable Update button and file input while active; re-enable when status returns to idle or after a failed/closeable terminal state on this card.

### 3. Phase label rename only in UI

Rename the visible span text to **RF firmware update**. No API field rename required.

### 4. Maintenance REBOOT = admin POST + two confirms

Add `POST /api/reboot` (or equivalent) requiring admin session; schedules host restart via existing `SettingsManager` pending-restart or direct delayed `ESP.restart` (same pattern as other deferred restarts). UI: two stacked confirm dialogs (or one dialog that advances to a second prompt). Cancel at either step aborts.

### 5. Login autofocus

In `showLoginForm` (and initial login paint on `index.html` / `user.html`), `loginUserName.focus()` after the overlay is shown. Keep existing empty-password submit behavior that focuses password.

### 6. MQTT save without host restart

`handleMqttPost`: `saveMain(false)`. Then force runtime apply:
- Tear down / recreate local broker when port, credentials, or serverType require it (today `MqttBroker::dispatch` only creates when `server == nullptr`; delete server when settings that affect listen change so dispatch rebuilds).
- Disconnect remote MQTT client and clear backoff so `dispatch` reconnects with new server/port/user.
- Update success text to “Saved. MQTT settings applied.” (no restart claim).

**Alternative considered:** Keep restart for serverType changes only — rejected; proposal says apply without restart for MQTT settings changes.

### 7. ZigBee save without host restart

`handleZigbeePost`: `saveMain(false)`; update `INTER_CHIP_HOST.setSettingsSource(...)`; request an immediate settings push to the slave (new small public API on `InterChipHost`, e.g. clear `settingsQueued` + `maybePushSettings`, only when bringup is normal).

Slave: change `onSlaveSettings` so a second settings delivery while the coordinator is already started **applies** the new channel (restart/reconfigure radio stack on the slave as needed) instead of returning early. Permit-join-on-boot is storage + push; runtime permit-join remains the existing command path.

**Fragile:** `InterChipHost` / possibly `InterChipSlave` are influence paths of `slave-spi-ota`. Apply must obtain `I approve fragile zone slave-spi-ota` before editing those files. Prefer the smallest possible surface (push trigger + slave apply-when-already-started).

**Alternative considered:** Soft-reset the slave EN to re-run bringup — works without changing `onSlaveSettings`, but is more disruptive; prefer targeted re-apply if the Zigbee stack supports it, else slave radio restart without host reboot.

### 8. LED6 5 Hz

Change `StatusRgb::kUpdateBlinkHalfMs` from `50` to `100`. Manual SPI-phase toggle already uses the same constant — both paths stay matched. No `FirmwareOta` edits.

## Risks / Trade-offs

- [Slave ignores channel after start] → Explicit slave apply path; verify channel change on hardware.
- [MQTT broker port change while clients connected] → Tear down and recreate listen socket; brief disconnect expected.
- [Fragile influence edits] → Gate InterChipHost/Slave edits on zone approval; do not touch `FirmwareOta`.
- [Update UI during reboot] → Card may unload when host reboots; reboot-wait ping/reload behavior stays as today, just on-card.
- [Double-confirm UX friction] → Intentional for destructive reboot.

## Migration Plan

Ship in one firmware build (`3.3.25` already bumped at propose). No settings schema migration. Operators using an open Update dialog mid-update: after flash, new UI is card-based.

## Open Questions

None deferrable — fragile-zone approval is required at apply time before InterChipHost/Slave edits, not a spec unknown.
