# Design

## Context

See proposal.md for motivation. Observed error `Firmware OTA frame missing` is raised only in `FirmwareOta::sendLastSlaveFrame` when `lastSlavePayloadLength == 0`. Callers are the slave upload pump (`enqueueNextSlaveChunk` resend path) and `onSlaveFrameLost` (always calls `sendLastSlaveFrame(true)` when under the fail budget).

Joined packages with `slave.bin` enter `Phase::Slave` via `startSlaveUploadCycle` (clears the last-frame buffer) then `pump()` / `enqueueNextSlaveChunk` packs SPI OTA begin and sends it. Host-only packages never enter this path.

Two tasks touch this state concurrently:

- `loopTask`: `FIRMWARE_OTA.pump()` → `enqueueNextSlaveChunk` / `sendLastSlaveFrame`
- `hostSpiTask` (priority raised to 10 while `isUpdatingSlave()`): `INTER_CHIP_HOST.pump()` → timeouts / inbound → `FirmwareOta::onSpiFrame` → may clear `lastSlavePayloadLength` and/or call `onSlaveFrameLost` → `sendLastSlaveFrame`

A portMUX covers only part of pack/claim; begin-ACK clears `lastSlavePayloadLength` outside the mux, and `sendLastSlaveFrame` checks length without holding it. That TOCTOU matches intermittent “frame missing,” including early in a slave cycle when the operator has not yet seen a stable SPI begin/erase (logged “before SPI begins”).

Prior field logs also showed the same error after heavy `SPI firmware frame lost` once the buffer was already cleared—secondary symptom of the same unsafe resend-on-empty path.

Fragile zones `slave-spi-ota` and `firmware-update-process` cover the hard paths; apply requires explicit human approval naming those ids.

## Goals / Non-Goals

**Goals:**

- Eliminate hard-fail `Firmware OTA frame missing` caused by empty last-frame buffer before begin is staged, and by concurrent clear+resend races during slave SPI streaming.
- Keep begin/chunk/end SPI framing, reply timeouts (`kOtaBeginReplyTimeoutMs` / chunk / end), max frame-loss budget, and post-transfer version verify cycles behaviorally intact.
- Add enough log context to distinguish “empty buffer skip” from real pack/link failures.

**Non-Goals:**

- Changing joined ZIP layout, HTTP staging, or host-apply/reboot sequencing.
- Retuning SPI clock override or chunk sizes unless required to land the race fix (prefer not).
- Reworking slave-side apply/ACK drain beyond what host retry consistency needs.
- Broad InterChipHost refactor outside OTA last-frame / timeout interaction.

## Decisions

### 1. Treat empty last-frame on retry as non-fatal before begin staged

**Choice:** In `sendLastSlaveFrame` / `onSlaveFrameLost`, if `lastSlavePayloadLength == 0` and begin is not yet staged (`!beginQueued` or equivalent “begin not in buffer / not waiting”), do **not** call `fail("Firmware OTA frame missing")`. Return false / skip resend and let `enqueueNextSlaveChunk` pack begin on the next pump.

**Why:** The error string claims a missing frame that was never packed; aborting the whole joined update is wrong for that case and matches “fails before SPI begins.”

**Alternatives:** Keep hard-fail but only after beginAcked — rejected as still racing around first send. Serialize all OTA onto one task — larger fragile-zone change; reserved if mux approach is insufficient.

### 2. Serialize last-frame clear and resend decision under the existing OTA mux (or equivalent)

**Choice:** Hold `gFirmwareOtaMux` (or a dedicated OTA state lock) across: read length → decide resend vs pack-next → clear length on ACK → set length after pack, so `loopTask` and `hostSpiTask` cannot interleave “length>0 check” with “ACK cleared length” then `sendLastSlaveFrame`.

**Why:** Partial critical sections already exist (`slavePackClaimed`); extending them to the TOCTOU window is the smallest fix aligned with current structure.

**Alternatives:** Move `FirmwareOta::pump` slave streaming entirely into `hostSpiTask` so only one task packs/sends — cleaner long-term, higher blast radius inside the freeze. Prefer mux first.

### 3. After begin staged, empty buffer on lost-frame still must not invent SPI bytes

**Choice:** If begin was staged and length is unexpectedly 0 on loss/retry, log a warning with phase/begin/fail counters; count toward fail budget or recover by packing the next needed frame from RAM stream when safe; only `fail` with a precise message after budget exhausted or when the stream cannot be reconstructed. Do not keep the misleading “frame missing” as the primary abort for the pre-begin case.

**Why:** Spec requires no zero-length enqueue and accurate failure reasons.

### 4. Diagnostics, then behavior

**Choice:** On any empty-buffer retry skip, log one warning line including `beginQueued`, `beginAcked`, `slaveFrameFails`, `waitingForSlaveResult`, and `imageOffset` (or equivalents). Keep volume low (not per-byte).

**Why:** Confirms the race in the field without changing SPI timing.

### 5. Fragile-zone gate before apply

**Choice:** Planning only now. Implementation tasks start only after human approval of `slave-spi-ota` and `firmware-update-process`, and allow-list entries if hooks require them.

## Risks / Trade-offs

- **[Risk] Skipping empty-buffer fail masks a real stuck state** → Mitigation: still fail after frame-loss budget or when begin cannot be packed; add counters/logs; do not spin forever.
- **[Risk] Wider critical sections lengthen interrupt disable** → Mitigation: keep mux sections to flag/length/memcpy bounds only; do not hold mux across SPI transfer or LittleFS.
- **[Risk] Moving work onto hostSpiTask regresses Wi-Fi/HTTP** → Mitigation: prefer mux on existing split; avoid moving `pump()` unless mux fails validation.
- **[Risk] Fragile-zone edit regresses OTA throughput/ACKs** → Mitigation: no timeout/clock/chunk-size changes in the first fix; validate with joined upload containing slave, host-only upload, and intentional chunk loss within budget.

## Migration Plan

1. Obtain fragile-zone approval (`slave-spi-ota`, `firmware-update-process`).
2. Land host fix; bump firmware build on apply.
3. Flash joined image; repeat slave-containing updates; confirm absence of pre-begin `Firmware OTA frame missing` and successful begin ACK → image stream.
4. Rollback: reflash prior joined build; no data migration.

## Open Questions

- None deferrable: apply still needs explicit fragile-zone approval before code edits.


### 6. Single in-flight OTA seq (4.0.40)

**Choice:** Set `waitingForSlaveResult` before `tryEnqueue`; refuse new OTA enqueue while `hasPending && pendingCmd == SpiCmdFirmwareOta`. Apply CmdResult/timeout only when `seq` matches `lastSlaveSeq`.

**Why:** Field logs showed `ignore result seq=N expect=N+1` with RF at 0% — host had raced a second seq ahead of the in-flight ACK.

### 7. END NACK vs timeout (4.0.39)

**Choice:** Distinguish NACK vs timeout in `onSlaveFrameLost`; complete-image verify escape only for END **timeouts**, not NACKs. Slave idempotent-ACK END if apply already armed.
