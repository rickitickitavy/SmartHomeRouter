# Proposal

## Why

After a successful slave SPI firmware transfer, the host often fails to observe the new slave version within 15 s, then resets the slave and immediately re-sends the image—even though the slave already runs the new build (visible after a router reboot). The main gap is missing a post-reset ready + version re-check before SPI resend. The Update dialog also auto-hides its request-result plates, and host LED6 stops blinking during SPI transfer when operators need that progress cue.

## What Changes

- Correct the slave update verify cycle: after SPI transfer, wait up to 15 s for version; on failure reset the slave, **wait until the slave is ready, read the version again**, and **only if the version is still wrong** repeat the SPI send (within the existing attempt budget). Status polling during verify remains required so those reads can succeed.
- Keep the firmware Update dialog’s request-result plates (`firmwareRebootSuccess` / `firmwareRebootError`) visible until the operator closes the dialog; do not apply the global 10-second auto-hide to those plates.
- Keep host LED6 blinking at the usual 10 Hz update cadence during slave SPI packet transfer by turning off the normal StatusRgb update blink before SPI transfer and toggling LED6 manually between SPI packets; restore the usual blink mechanism after SPI transfer ends.

## Capabilities

### New Capabilities

- _(none)_

### Modified Capabilities

- `joined-firmware-ota`: Slave update cycle with post-transfer verify, post-reset ready+version re-check, and SPI resend only when version is still wrong.
- `host-slave-spi`: While joined OTA is verifying the slave (not streaming OTA frames)—including after a host-initiated verify reset—the host MUST poll status and complete bringup so version can be read before any resend.
- `web-console`: Exception to the global save-error/save-success auto-hide for the Update firmware dialog result plates.
- `status-rgb-led`: Host LED6 update indication MUST continue at 10 Hz during the slave SPI transfer phase via between-packet manual blink.

## Impact

- Host OTA pipeline (`FirmwareOta` verify/reset/re-upload sequencing), `InterChipHost` status/keepalive gating while OTA is busy but not streaming slave frames, `StatusRgb` / SPI transfer path for LED6, and Update dialog JS in `data/index.html`.
- **Fragile zones:** Implementation touches hard-frozen `slave-spi-ota` / `firmware-update-process` (`FirmwareOta.*` and influence on `InterChipHost`). Apply MUST wait for explicit human approval naming those zone ids before editing frozen paths.
- Propose bumped `FIRMWARE_VERSION` build to `3.1.36` (apply will bump again when source lands).
