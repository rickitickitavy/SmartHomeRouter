## Context

User requirement only: update Hosted C6 when the update is **present** and **needed**. Prior over-scope (Status `hostedHasUpdate` spam, extra product surfaces) is rejected.

## Goals / Non-Goals

**Goals:**

- Optional ZIP members `hosted-c6.bin`, `hosted-c6.version.txt`.
- P4: skip if absent; skip if version match; apply over SDIO if mismatch (after full ZIP stage).
- Join accepts optional Hosted inputs.
- Host LED3 blue web-request indicator on S3 and P4.

**Non-Goals:**

- Embedding Hosted image in `host.bin`.
- Periodic Status calls to `hostedHasUpdate()`.
- New Status fields for Hosted version.
- Changing Zigbee slave SPI OTA protocol.
- S3 Hosted programming.
- Engineer-only update as the field path.

## Decisions

1. Third file in joined ZIP, not embed.
2. Need = file present ∧ readable running version ∧ ≠ package version; absent → skip; unreadable/missing version text when file present → fail that step (do not guess).
3. Order: after stage, Hosted (if required) then existing slave then host.
4. Version string form matches Hosted major.minor.patch (e.g. `2.12.11`).
5. Host LED3 is blue; ON for any in-flight HTTP request (server middleware + disconnect) and WebSocket connect/data frames; OFF when that unit finishes; refcounted for overlap.

## Risks / Trade-offs

- [Risk] Hosted SDIO may drop Wi‑Fi → Mitigation: only after ZIP fully staged.
- [Risk] Fragile Update path → Mitigation: zone approval before code; no SPI protocol edits.

## Open Questions

- None for scope; API to read running Hosted version once at prepare time (not on Status poll).
