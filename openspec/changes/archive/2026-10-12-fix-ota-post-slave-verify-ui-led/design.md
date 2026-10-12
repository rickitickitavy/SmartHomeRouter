# Design

## Context

See proposal.md — Why. Observed code today:

- After slave SPI image transfer, `FirmwareOta` enters `VerifyingSlave` and waits up to 15 s for `slaveVersionMatchesExpected()`. On failure it sets `slaveVerifyResetPending`, resets the slave, then in `slaveAwaitingLinkForReupload` calls `startSlaveUploadCycle()` as soon as `isNormal()` — **without waiting for a fresh version read**. That is the main incorrect recovery when the first transfer already succeeded.
- `InterChipHost::pump()` enqueues `SpiCmdGetStatus` only when `HostBringupNormal && !FIRMWARE_OTA.busy()`. `VerifyingSlave` sets `busy()` true, so `slaveVersionText` often never refreshes during verify (compounding the false re-upload).
- Update dialog plates `firmwareRebootSuccess` / `firmwareRebootError` use `save-success` / `save-error` and are armed by the global 10 s auto-hide.
- Host LED6 update blink is driven by `StatusRgb::service()` while `updateHeld` is set from `FIRMWARE_OTA.busy()`. Operator wants explicit between-packet LED6 toggles at 10 Hz (`kUpdateBlinkHalfMs` = 50 ms) during SPI streaming.

Fragile zones `slave-spi-ota` and `firmware-update-process` cover `FirmwareOta.*` and influence `InterChipHost`; apply requires named human approval before those edits.

## Goals / Non-Goals

**Goals:**

- Implement the full slave update verify cycle: SPI done → wait ≤15 s for version → on fail reset → wait ready → read version again → SPI resend **only if still wrong**.
- Allow SPI bringup + status reads during all verify waits (post-transfer and post-reset) without weakening exclusive OTA-frame gating during streaming.
- Keep Update-dialog reboot result plates visible until Close / new update.
- Keep LED6 blinking at 10 Hz throughout slave SPI packet transfer via manual between-packet toggles.

**Non-Goals:**

- Changing joined ZIP layout, `version.txt` join tooling, or ADR 0008 slave-then-host order.
- Changing SPI OTA begin/chunk/end ACK/retry timing algorithms beyond what verify/LED need.
- Redesigning global plate auto-hide for non-Update screens.
- Changing slave LED6 pairing behavior.

## Decisions

### 1. Version sample required before SPI resend; silence → reset (20 s × 5)

- **Choice:** After each SPI transfer, clear cached version and wait up to **20 s** for a non-empty status version while bringup can complete. Match → success. Non-empty mismatch → SPI resend (only because a sample exists). No sample within 20 s → **reset** (do not resend), wait again; at most **5** such no-response resets, then fail the update.
- **Why:** Operator rule: never resend without reading the current slave version; silence means reset/retry, not blind re-flash.
- **Alternatives:** Resend on Normal alone — rejected. Treat empty version as mismatch and resend — rejected (that is silence).

### 2. Allow status (and bringup keepalive) during all `VerifyingSlave` waits, not during `Slave` streaming

- **Choice:** Treat `isUpdatingSlave()` (phase `Slave`) as the exclusive OTA stream gate. While verifying (post-transfer or post-reset), permit `GetStatus` / ping / time-sync and normal ready→settings→Normal bringup.
- **Why:** Without status, both verify windows cannot see the new version.
- **Alternatives:** One-shot `GetStatus` from OTA only — too weak after reboot; needs repeated polls through bringup.

### 3. Clear stale version when entering each verify wait

- **Choice:** Clear cached `slaveVersionText` (or treat empty as “not yet sampled”) when entering post-transfer verify and again after verify reset, so a pre-OTA string cannot false-fail or false-pass.
- **Why:** Stale old version ≠ expected looks like failure; stale old version accidentally equal to expected would be wrong after a failed flash (rare). Fresh sample is required.

### 4. Exempt Update-dialog result plates from auto-hide in JS

- **Choice:** Do not arm `armFeedbackPlateAutoHide` for `#firmwareRebootSuccess` and `#firmwareRebootError`.
- **Why:** Minimal UI change; preserves global 10 s hide elsewhere.

### 5. Pause StatusRgb update blink; toggle LED6 between SPI packets

- **Choice:** Before slave SPI streaming, pause automatic update blink; between SPI OTA packets toggle LED6 at 50 ms half-period; after streaming ends restore normal `updateHeld` blink for remaining update phases.
- **Why:** Operator-requested; same cadence as `kUpdateBlinkHalfMs`.

### 6. Fragile-zone gate before apply

- **Choice:** Verify/resend sequencing and any `FirmwareOta` / OTA-influencing `InterChipHost` / in-transfer LED hooks wait for named approval of `firmware-update-process` and `slave-spi-ota`.

## State sketch (verify)

```text
SPI transfer done
  → VerifyWait (≤20s, status polled, version cache cleared)
       match           → Host apply / Done
       non-empty wrong → SPI resend → VerifyWait (resets counter cleared)
       no sample       → if resets < 5: reset + VerifyWait; else Fail
```

## Risks / Trade-offs

- [Risk] Status/ping during verify interferes with a still-finishing slave flash commit → Mitigation: only after streaming ends (`!isUpdatingSlave()`); keep slave apply-drain before restart.
- [Risk] Extra 15 s after each failed first wait lengthens unhappy path → Mitigation: accepted; happy path after reset often finds correct version and skips resend (faster overall than a full re-upload).
- [Risk] Manual LED6 races `StatusRgb::apply()` → Mitigation: pause auto update blink during streaming; single LED6 writer in that window.
- [Risk] `version.txt` ≠ built slave `FIRMWARE_VERSION` → Mitigation: log seen vs expected; join must embed matching version.
- [Trade-off] Keep attempt budget so a truly failed commit can still recover via SPI resend after post-reset mismatch.

## Migration Plan

- Ship in a normal joined firmware build (`FIRMWARE_VERSION` already bumped at propose to `3.1.36`; apply bumps again when code lands).
- No settings/schema migration. Rollback = previous firmware build.

## Open Questions

- None material for specs/tasks; confirm fragile-zone approval wording at apply time only.
