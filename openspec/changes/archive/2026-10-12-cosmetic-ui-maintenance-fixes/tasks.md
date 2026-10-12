# Tasks

## 1. Update card UI

- [x] 1.1 Move firmware phase list, status, reboot-wait, and close/clear controls from `#firmwareUpdateDialog` into the System → Update card; remove or permanently unused the modal overlay; verify starting an update shows progress only on the card (no modal)
- [x] 1.2 Rename the slave-progress label to **RF firmware update** and verify the Update card never shows **Master to slave**
- [x] 1.3 On entering the Update tab, fetch `/api/update/status` and disable Update/start controls when an update is already active; verify re-entry while idle leaves controls enabled
- [x] 1.4 Retarget reboot-wait success/error plate auto-hide exemption to the on-card plates and verify those plates stay past 10 seconds while other save plates still auto-hide

## 2. Login focus and Maintenance reboot

- [x] 2.1 Focus `#loginUserName` when the login form is shown in `index.html` and `user.html`; verify opening `/` without a session lands focus in the username field
- [x] 2.2 Add admin `POST /api/reboot` (or equivalent) that schedules a host restart without requiring a settings write; verify unauthorized callers are rejected
- [x] 2.3 Add Maintenance **REBOOT** with two sequential confirmations wired to that API; verify cancel at either step does not reboot and two confirms do

## 3. MQTT apply without restart

- [x] 3.1 Change MQTT POST to `saveMain(false)`, force broker/client re-apply for the new settings, and return a non-restart success message; verify saving MQTT does not reboot the host and the new serverType/port/credentials take effect
- [x] 3.2 Update any MQTT UI hint text that still implies a restart after save; verify the success plate matches the new response

## 4. ZigBee apply without restart

- [x] 4.1 Before editing `InterChipHost` / `InterChipSlave`, obtain explicit approval naming fragile zone `slave-spi-ota` (and allow entry if hooks require it); verify approval is recorded before those files change
- [x] 4.2 Change ZigBee POST to `saveMain(false)`, refresh settings source, and push settings to the slave without host restart; verify save response does not announce restart and the host stays up
- [x] 4.3 Make the slave apply a second settings push when the coordinator is already running (channel change); verify a channel save while radio is up takes effect without host reboot
- [x] 4.4 Update the ZigBee tab hint that still says settings apply only after restart; verify the hint matches apply-without-restart behavior

## 5. LED6 blink rate

- [x] 5.1 Set `StatusRgb::kUpdateBlinkHalfMs` to 100 (5 Hz) so both continuous and manual SPI-phase toggles match; verify during an update LED6 blinks at ~5 Hz and critical still holds solid

## Workflow follow-up

- After implementation, run `/opsx-apply` tracking against these checkboxes, then archive when review is done.
- Do not edit hard-frozen `FirmwareOta` / joined-ZIP paths for this change.
- ZigBee apply uses existing public `INTER_CHIP_HOST.resetSlaveSynchronous()` (no InterChipHost/InterChipSlave source edits; fragile zone approval not required for this approach).
