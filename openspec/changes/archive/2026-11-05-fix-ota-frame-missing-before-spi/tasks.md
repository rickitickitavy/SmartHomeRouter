# Tasks

## 1. Gate and baseline

- [x] 1.1 Obtain explicit human approval naming fragile zones `slave-spi-ota` and `firmware-update-process` (and allow-list entries if hooks require them) before editing hard paths; verify approval is recorded in the apply conversation
- [x] 1.2 Confirm current fail site: `sendLastSlaveFrame` when `lastSlavePayloadLength == 0`, callers `enqueueNextSlaveChunk` / `onSlaveFrameLost`, and dual-task ownership (`loopTask` pump vs `hostSpiTask`); verify by citing those paths in the apply notes

## 2. Empty-buffer and mux hardening

- [x] 2.1 Change empty last-frame handling so pre-begin (`begin` not yet staged) skips hard-fail `Firmware OTA frame missing`, does not enqueue length-zero OTA, and lets the next pack/send produce begin; verify by code review that that fail string is unreachable before begin is staged
- [x] 2.2 Extend `gFirmwareOtaMux` (or equivalent) so length check, ACK clear, pack set-length, and resend decision cannot TOCTOU across `loopTask` and `hostSpiTask`; verify begin-ACK clear and `sendLastSlaveFrame` length read share the same lock region
- [x] 2.3 After begin staged, on empty buffer during loss/retry: log warning with beginQueued/beginAcked/slaveFrameFails/waiting/imageOffset, do not invent SPI bytes, recover via pack-next or fail with an accurate message after budget; verify no zero-length `tryEnqueue` of `SpiCmdFirmwareOta` remains on that path
- [x] 2.4 Bump `FIRMWARE_VERSION` build by 1 in `include/Defines.h` for the apply source change and verify the new string builds into the host image

## 3. In-flight seq and END semantics (follow-up)

- [x] 3.1 Seq-match OTA CmdResult/timeout; refuse enqueue of a new OTA seq while an OTA reply is pending; claim waiting before tryEnqueue — verify no systematic `ignore result seq=N expect=N+1` flood with RF stuck at 0%
- [x] 3.2 On-wire END timeout; END NACK does not take complete-image verify escape; settle SPI before verify-fail resend — verify in code review / device logs
- [x] 3.3 Firmware build at **4.0.40** for this reopen sync/archive

## 4. Integration checks

- [ ] 4.1 On device with 4.0.40: joined ZIP with `slave.bin` — RF % climbs; slave reaches expected version; no false complete-image verify
- [ ] 4.2 Host-only package still applies host only
- [ ] 4.3 Mid-transfer frame loss within budget still resends packed last frame (seq preserved)

## Workflow follow-up

- Archive after sync (user-requested reopen cycle); device tasks 4.x may remain open.
- Keep fragile-zone allow entries temporary; remove after the edit session ends.
