# Tasks

## 1. Fragile-zone gate

- [x] 1.1 Before editing `FirmwareOta.*` or OTA-influencing `InterChipHost` / SPI transfer LED hooks, obtain explicit human approval naming `firmware-update-process` and `slave-spi-ota`, add those zone ids to `.cursor/fragile-zones.allow`, and verify the allow file lists both ids

## 2. Post-slave version verify (SPI + OTA)

- [x] 2.1 Change host keepalive/status enqueue so `GetStatus` (and needed ping/time-sync) run during all `VerifyingSlave` waits (post-transfer and post-reset) while remaining gated off during exclusive `Slave` SPI streaming; verify with logs that status frames arrive during verify and do not interleave during streaming
- [x] 2.2 Ensure post-commit and post-reset slave reboot can complete ready→settings→Normal during verify (bringup not blocked by OTA `busy()`); verify slave returns to Normal without a full gateway reboot
- [x] 2.3 Implement post-transfer verify (≤15 s): clear stale version on entry; on match proceed to host apply / slave-only done; verify a successful first transfer with readable version does not SPI-resend
- [x] 2.4 On post-transfer verify failure: reset slave, enter post-reset verify (wait ready, clear stale version, poll version ≤15 s); if version matches, proceed **without** SPI resend; verify this path with a case where the first wait failed but the slave already has the expected version
- [x] 2.5 Only if post-reset version is still wrong, start SPI re-upload then return to post-transfer verify; remove “Normal alone ⇒ `startSlaveUploadCycle`”; verify a deliberate wrong image/version still resends and eventually fails within the attempt budget without applying host

## 3. Update dialog result plates

- [x] 3.1 Exempt `#firmwareRebootSuccess` and `#firmwareRebootError` from `armFeedbackPlateAutoHide` / `armAllFeedbackPlates`; verify those plates stay visible past 10 s until Close, and that a normal settings save plate still auto-hides

## 4. LED6 blink during slave SPI transfer

- [x] 4.1 Add StatusRgb support to pause automatic update blink and to toggle host LED6 on demand at the same 50 ms half-period; verify auto blink stops when paused and manual toggle drives LED6
- [x] 4.2 Before slave SPI streaming, pause auto update blink; between SPI OTA packets toggle LED6 at 10 Hz; after streaming ends restore normal `updateHeld` blink for remaining update phases; verify LED6 visibly blinks throughout slave transfer and critical still forces solid LED6

## 5. Firmware version

- [x] 5.1 Bump `FIRMWARE_VERSION` build by 1 for the apply source change and verify `include/Defines.h` shows the new build

## Workflow follow-up

- Archive the change after review when implementation is done.
- Remove temporary `.cursor/fragile-zones.allow` entries after the session unless the human keeps them.
