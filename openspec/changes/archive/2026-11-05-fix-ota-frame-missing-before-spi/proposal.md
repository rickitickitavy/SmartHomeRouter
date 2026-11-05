# Proposal

## Why

Joined updates that include `slave.bin` often fail with host log `ERROR: Firmware OTA frame missing` before the operator sees a stable slave SPI begin/erase transfer. The failure aborts the joined update so the host never applies, and retries are flaky. The error is raised only when the host tries to (re)send a slave OTA SPI frame while its last-frame buffer length is already zero—commonly from a race between `loopTask` (`FirmwareOta::pump`) and `hostSpiTask` (`InterChipHost::pump` → `FirmwareOta::onSpiFrame` / `onSlaveFrameLost`).

## What Changes

- Harden the host slave-OTA send/retry path so a missing last-frame buffer cannot abort the update as a hard error when SPI begin has not yet been successfully staged, and so retry/ACK handling cannot clear and then resend a zero-length frame.
- Make last-frame buffer lifetime and `waitingForSlaveResult` updates consistent across the two tasks that touch slave OTA state (critical section / single owner for clear+resend decisions).
- Keep the joined slave-then-host SPI begin/chunk/end protocol, timeouts, and version-verify cycles unchanged in product behavior; this is a reliability fix for an existing failure mode.
- Add diagnostic log context when a resend is skipped or aborted because the frame buffer is empty (phase, beginQueued/beginAcked, fail count) so future regressions are easier to confirm.
- **Fragile zones:** implementation will edit `slave-spi-ota` / `firmware-update-process` hard paths (`src/FirmwareOta.cpp`, `include/FirmwareOta.h`, and possibly `InterChipHost` influence). Apply MUST wait for explicit human approval naming those zone ids.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `joined-firmware-ota`: Require that a joined package containing `slave.bin` does not fail with “OTA frame missing” before the first slave SPI begin frame has been successfully queued/sent; empty last-frame buffer on retry must not hard-fail the update in that window.
- `host-slave-spi`: Require host firmware-OTA retry/timeout handling to treat an empty last-frame buffer as a non-fatal skip or controlled abort of that retry only after begin was staged, without inventing a synthetic SPI frame; concurrent pump/onSpiFrame must not clear and resend inconsistently.

## Impact

- Host firmware OTA orchestration (`FirmwareOta`), and possibly host SPI queue/timeout interaction (`InterChipHost`) for `SpiCmdFirmwareOta`.
- Operator-visible Update reliability for packages that include slave; host-only packages already skip this path.
- No change to joined ZIP layout, web upload API, or slave apply/commit protocol framing.
- Firmware version build bumped to `4.0.35` during propose; apply will bump again when source lands.
- Apply is gated by fragile-zone approval for `slave-spi-ota` and `firmware-update-process`.
