# Design: Clamp settings import to firmware bounds

## Context

`POST /api/settings/restore` applies a `hardware` object via `applyHardwareJson`. That helper currently **returns false** (HTTP 400) when history days/sweep/size/intervals are outside firmware constants. Operators restoring cross-build or hand-edited exports then lose the Hardware slice (or the whole restore step fails for hardware).

Directed rule (option 1): clamp to **declared firmware min/max** for the running image — not a PSRAM-safe allocator max. Values inside constants but still too large to allocate (e.g. S3 `24576` with `HISTORY_MAX_SIZE_KIB_MAX` 65534) remain a separate problem.

## Goals / Non-Goals

**Goals**

- On settings restore/import, out-of-range bounded numerics snap to min or max and restore continues.
- Use SoC-specific constants where they already differ (e.g. `HISTORY_MAX_SIZE_KIB_MAX` on P4 vs S3).
- Prefer one Hardware JSON apply implementation so restore and Hardware POST do not diverge (POST also clamps those fields).

**Non-Goals**

- Capping history size to allocatable PSRAM / “safe” slab size.
- Changing Wi-Fi exclusion on restore.
- Slave changes.

## Decisions

1. **Clamp rule:** `value > max → max`; `value < min → min`; in-range unchanged.
2. **Fields in scope (Hardware apply / restore):** at least history max store days, sweep interval, max size KiB, SD/flash backup intervals; SPI/LED/queue already clamp or stay as today — align any remaining reject-on-range Hardware numerics to the same clamp rule when touched by restore.
3. **Entry size (discrete):** invalid → nearest allowed among `{128,256,384,512,768,1024,1536,2048}` (ties → factory default 1024).
4. **Logging:** MAY log when a field was clamped (field name + from/to).
5. **Host version:** bump host build on propose; slave unchanged.

## Risks / Trade-offs

- Silent clamp can hide a bad export; mitigated by optional log and Status/Hardware showing the clamped value after restore.
- Does not fix empty Historical storage when size is within max but too large for PSRAM.

## Migration

- No file format change. Old exports with out-of-range numbers restore successfully with clamped values.

## Open Questions

- None for option 1.
