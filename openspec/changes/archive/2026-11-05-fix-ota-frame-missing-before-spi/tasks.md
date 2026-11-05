# Tasks

## 1. Gate and baseline

- [x] 1.1 Obtain explicit human approval naming fragile zones `slave-spi-ota` and `firmware-update-process` (and allow-list entries if hooks require them) before editing hard paths; verify approval is recorded in the apply conversation
- [x] 1.2 Confirm current fail site: `sendLastSlaveFrame` when `lastSlavePayloadLength == 0`, callers `enqueueNextSlaveChunk` / `onSlaveFrameLost`, and dual-task ownership (`loopTask` pump vs `hostSpiTask`); verify by citing those paths in the apply notes

## 2. Empty-buffer and mux hardening

- [x] 2.1 Change empty last-frame handling so pre-begin (`begin` not yet staged) skips hard-fail `Firmware OTA frame missing`, does not enqueue length-zero OTA, and lets the next pack/send produce begin; verify by code review that that fail string is unreachable before begin is staged
- [x] 2.2 Extend `gFirmwareOtaMux` (or equivalent) so length check, ACK clear, pack set-length, and resend decision cannot TOCTOU across `loopTask` and `hostSpiTask`; verify begin-ACK clear and `sendLastSlaveFrame` length read share the same lock region
- [x] 2.3 After begin staged, on empty buffer during loss/retry: log warning with beginQueued/beginAcked/slaveFrameFails/waiting/imageOffset, do not invent SPI bytes, recover via pack-next or fail with an accurate message after budget; verify no zero-length `tryEnqueue` of `SpiCmdFirmwareOta` remains on that path
- [x] 2.4 Bump `FIRMWARE_VERSION` build by 1 in `include/Defines.h` for the apply source change and verify the new string builds into the host image

## 3. Integration checks

- [ ] 3.1 On device, upload a joined ZIP **with** `slave.bin` repeatedly and verify logs show slave SPI transfer cycle → begin queued/acked → image progress without pre-begin `Firmware OTA frame missing`
- [ ] 3.2 Upload a host-only package (no `slave.bin`) and verify host-only apply still succeeds unchanged
- [ ] 3.3 Confirm a normal mid-transfer frame loss within budget still resends the packed last frame (seq preserved) and does not abort solely for empty buffer

## Workflow follow-up

- Archive the change after review and successful device validation.
- Keep fragile-zone allow entries temporary; remove after the edit session ends.
