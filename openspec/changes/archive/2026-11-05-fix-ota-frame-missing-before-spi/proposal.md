# Proposal

## Why

Joined updates that include `slave.bin` often fail with host log `ERROR: Firmware OTA frame missing` before the operator sees a stable slave SPI begin/erase transfer. The failure aborts the joined update so the host never applies, and retries are flaky. The error is raised only when the host tries to (re)send a slave OTA SPI frame while its last-frame buffer length is already zero—commonly from a race between `loopTask` (`FirmwareOta::pump`) and `hostSpiTask` (`InterChipHost::pump` → `FirmwareOta::onSpiFrame` / `onSlaveFrameLost`).

## What Changes

- Harden the host slave-OTA send/retry path so a missing last-frame buffer cannot abort the update as a hard error when SPI begin has not yet been successfully staged, and so retry/ACK handling cannot clear and then resend a zero-length frame.
- Make last-frame buffer lifetime and `waitingForSlaveResult` updates consistent across the two tasks that touch slave OTA state (critical section / single owner for clear+resend decisions).
- Enforce a single in-flight OTA sequence (no enqueue-ahead while a reply is pending; ignore late ACKs for other seqs) so image offset and RF progress stay correct.
- Use on-wire END flags for the long END reply timeout; do not treat repeated END NACKs as “complete image”; settle SPI before verify-fail full resend cycles.
- Keep the joined slave-then-host SPI begin/chunk/end protocol framing unchanged.
- **Fragile zones:** `slave-spi-ota` / `firmware-update-process` (approved in apply).

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `joined-firmware-ota`: Require that a joined package containing `slave.bin` does not fail with “OTA frame missing” before the first slave SPI begin frame has been successfully queued/sent; empty last-frame buffer on retry must not hard-fail the update in that window.
- `host-slave-spi`: Empty last-frame handling; single in-flight OTA seq with seq-matched results; END NACK must not fake complete-image verify.

## Impact

- Host firmware OTA orchestration (`FirmwareOta`), and possibly host SPI queue/timeout interaction (`InterChipHost`) for `SpiCmdFirmwareOta`.
- Operator-visible Update reliability for packages that include slave; host-only packages already skip this path.
- No change to joined ZIP layout, web upload API, or slave apply/commit protocol framing.
- Firmware build through apply/follow-ups: **4.0.40**.
