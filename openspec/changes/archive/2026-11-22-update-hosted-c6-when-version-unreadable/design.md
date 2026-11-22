## Context

`FirmwareOta::prepareJoinedPackage` (P4) today:

1. Require `hosted-c6.version.txt` if `hosted-c6.bin` is present (else fail).
2. `readRunningHostedC6Version` via `hostedGetSlaveVersion` — fails if Hosted not initialized or major/minor/patch all zero.
3. On read failure → `fail("Hosted C6 version unreadable")` and abort prepare.
4. On match → skip; on mismatch → `hostedC6UpdateRequired = true`.

Operator rule: **if get version of HOSTED C6 failed → update anyway.**

## Goals / Non-Goals

**Goals:**

- Unreadable running Hosted version + valid package Hosted members → set update required and continue prepare (do not fail the session for that reason alone).
- Keep skip-on-match and apply-on-mismatch when the running version **is** readable.
- Keep failing when package `hosted-c6.version.txt` is missing/empty.

**Non-Goals:**

- Changing Hosted SDIO transfer / activate sequence itself.
- Polling Hosted version on Status.
- Softening missing package version text.
- S3 or slave OTA behavior.

## Decisions

1. **Unreadable running version ⇒ update required**  
   Log a warning (no secret data) that version was unreadable and Hosted update will proceed. Set `hostedC6UpdateRequired = true`. Do not call `fail` for that case.

2. **What counts as unreadable**  
   Same as today’s `readRunningHostedC6Version` false return (Hosted not initialized, or 0.0.0 triple, or empty formatted string).

3. **Package version still mandatory**  
   Missing/empty `hosted-c6.version.txt` with `hosted-c6.bin` present remains a hard fail — without a package version there is no post-update identity string and no match-based skip.

4. **Fragile zone**  
   Implementation edits `FirmwareOta` under `firmware-update-process`. Apply needs explicit zone approval; do not change `slave-spi-ota` / SpiProtocol framing.

5. **Host version**  
   Propose host build **4.6.03**. Slave unchanged.

## Risks / Trade-offs

- **[Risk] Unnecessary Hosted reflash** when version API flakes but C6 is already current → Mitigation: preferred over blocking Update; match skip still works when read succeeds.
- **[Risk] Update still fails later** if SDIO program cannot run because Hosted is truly down → Mitigation: existing Hosted apply errors remain; only the early abort on version read is removed.

## Migration Plan

- Flash host **4.6.03**. No settings migration. Joined ZIP format unchanged.

## Open Questions

None — operator directed “update anyway” when Hosted version get fails.
