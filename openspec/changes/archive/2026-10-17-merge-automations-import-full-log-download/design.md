# Design

## Context

See proposal.md — Why.

**Import all:** UI confirms “Import replaces all automations…”, then `POST /api/automations/restore` → `AutomationStore::replaceAllFromJson`, which builds a new list and swaps it in—dropping any name not in the file. Spec today says replace; product now wants merge/upsert-by-name and keep locals.

**Log download:** Ring target is `Logger::kRingCapacityTarget` (512 KiB) in PSRAM with 64 KiB internal fallback. `GET /api/log` intentionally uses `snapshotRingTail(kWebViewMaxBytes)` (64 KiB). `GET /api/log/download` already calls `snapshotRing` for the full used length; if operators still see ~64 KiB files, find and fix truncation (chunked response indexing, client `response.text()`/memory, wrong endpoint, or ring not actually in PSRAM). Do not raise the on-screen view cap unless needed for the download fix.

## Goals / Non-Goals

**Goals:**
- Import all merges; no wipe of missing names.
- Download full log delivers the entire current ring used length.
- Clear confirm wording.

**Non-Goals:**
- Changing settings-export restore of automations (full settings restore may still replace the automations group—call out if it shares `replaceAllFromJson`; add `mergeAllFromJson` for Import all only).
- Expanding the on-screen Log poll beyond 64 KiB.
- Increasing ring capacity beyond the existing PSRAM target.

## Decisions

1. **`mergeAllFromJson` (or equivalent) for Import all**  
   Parse file into records; for each name upsert into the live list; leave other live records alone; persist once. Reject invalid file without mutating.  
   *Alternatives:* keep replace with optional checkbox — rejected; human asked not to remove existing.

2. **Settings restore path**  
   If `/api/settings/restore` uses the same replace helper, leave that as replace-all for the automations slice of a full settings file; only Automations page Import all uses merge.

3. **Download full ring**  
   Keep `/api/log/download` on full `snapshotRing`; verify chunked streaming and client download produce `X-Log-Ring-Used` bytes. Prefer `Content-Length` or reliable chunked completion over loading into a String on the server. Fix client if it stops early.

## Risks / Trade-offs

- **[Risk] Import grows toward AUTOMATION_STORE_MAX** → Mitigation: reject when merge would exceed max; log clear error.
- **[Risk] Operators relied on replace-as-wipe** → Mitigation: confirm text; they can delete manually or import after clearing.
- **[Risk] Large download on low RAM clients** → Mitigation: chunked transfer; ring max 512 KiB.

## Migration Plan

- Host firmware + console assets. No store format change.
- Rollback: previous image restores replace-all import and prior download behavior.

## Open Questions

- None material; settings restore remains replace for automations group (assumed).
