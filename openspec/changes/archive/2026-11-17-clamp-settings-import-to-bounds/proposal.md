# Proposal

## Why

Global settings restore (and the Hardware JSON apply path it uses) currently **rejects** out-of-range numeric values (e.g. history days/size/intervals) with HTTP 400, so a settings file from another build or a mistyped bound can fail the whole Hardware slice. Operators need import to succeed by **clamping** each bounded value to this firmware’s declared min/max instead of failing the restore.

## What Changes

- On **settings import/restore**, for bounded numeric fields that this firmware defines with a min and max: if the imported value is **greater than max**, store **max**; if **less than min**, store **min**. Do **not** reject the restore solely for being outside that range.
- Use the **declared firmware constants** for the running image (S3 vs P4 where they differ). This is **not** a board-safe PSRAM allocator cap — values that are still within constants but too large to allocate remain possible (separate from this change).
- Prefer one shared apply path so Hardware POST that uses the same helpers gets the same clamp (no dual reject-vs-clamp behavior).
- Discrete hard-choice fields (history entry size 128/256/384/512/768/1024/1536/2048): map invalid values to the nearest allowed choice (or factory default if equidistant / non-numeric).

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: settings restore / Hardware JSON apply clamps out-of-range bounded numerics instead of rejecting.

## Impact

- Host only: `WebConsole` settings restore + `applyHardwareJson` (and any shared setters). Optional log when a value was clamped.
- No slave firmware changes.
- Propose host bump **4.3.31**.
