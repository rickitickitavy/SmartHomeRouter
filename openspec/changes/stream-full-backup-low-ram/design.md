# Design

## Context

- See proposal.md — Why.
- Today `handleFullBackupPost` builds settings JSON, calls `fullBackupBuildZip` → `StoreZipWriter::addFile` **`malloc`-copies every member into DRAM**, then `finish()` allocates the assembled ZIP (prefer PSRAM, else DRAM), and the HTTP handler holds that buffer in `OwnedZipBytes` until the client finishes downloading.
- Observed Free RAM drop (~69 KiB) matches packing members + holding the archive on the internal heap path Status reports (`ESP.getFreeHeap` / Free RAM).
- AsyncWebServer already supports callback `beginResponse` that fills a buffer per chunk; Full backup currently only uses that to copy out of a prebuilt ZIP.

## Goals / Non-Goals

**Goals**

- Emit a valid store-method ZIP with the same members as today while using a fixed **4–8 KiB** work/send buffer for archive generation and HTTP output.
- Never keep the **complete** ZIP (all members concatenated + central directory) resident in DRAM or as one download blob.
- Avoid DRAM duplication of each member solely for ZIP staging (`addFile` malloc copies).
- Preserve ZIP compatibility with Full restore / `StoreZipReader`.

**Non-Goals**

- Changing Full restore (still receives a ZIP body as today).
- Changing Export settings JSON download.
- Changing which slices/options Full backup includes.
- Reworking the live history PSRAM slab layout.
- Guaranteeing zero temporary PSRAM for a single large **member source** (e.g. settings JSON string, or a history snapshot blob used only as a stream source) — the forbidden object is the **whole archive**.

## Decisions

1. **Stream ZIP with data descriptors (ZIP bit 3)**  
   Write each local header with general-purpose bit 3 set, stream file bytes while computing CRC32, then write the data descriptor (CRC + sizes). Keep only small per-member metadata (path, CRC, sizes, local-header offset) for the central directory at the end.  
   *Alternative considered:* Pre-scan all sizes/CRCs then stream with known headers — needs either two passes over large members or still buffering them. Rejected for RAM and complexity.  
   *Alternative considered:* Write ZIP to LittleFS/SD temp then stream the file — uses flash wear and free FS space; rejected as primary approach (may remain an emergency fallback only if streaming proves impossible on a board, not the planned path).

2. **Fixed 4–8 KiB work buffer**  
   Use one host-side buffer in that range for (a) reading member sources in chunks and (b) filling AsyncWebServer response chunks. Prefer **8 KiB** when Free RAM allows at start of backup, else **4 KiB**. Do not grow with archive size.

3. **Replace `fullBackupBuildZip` + `OwnedZipBytes` download path**  
   Drive generation from the HTTP chunk callback (or a small stream state object owned for the response lifetime): emit next ZIP bytes into the caller buffer until EOF, then free state. Remove holding `result.zipBytes` for the whole transfer.

4. **Member sources without DRAM archive staging**  
   - `settings.json` / `manifest.json`: keep building in existing `PsramJsonBuffer` / small `String`; stream bytes from that source into the ZIP (do not `malloc`-copy into a second DRAM ZIP entry list).  
   - Scene bakes: read LittleFS in buffer-sized chunks into the ZIP (or reuse existing bake read only if it stays in PSRAM and is freed after that member finishes — prefer chunked file read).  
   - History: when included, stream into `history/snapshot.bin` from the preferred source (on-disk snapshot file when present; otherwise produce/stream snapshot bytes without also holding a finished ZIP). Do not `malloc` a second full DRAM copy of history for ZIP entry storage.

5. **Keep `StoreZipReader` for restore**  
   Reader still opens an in-memory ZIP for Full restore. Optional: leave a non-streaming writer only if other callers need it; Full backup MUST use the streaming path. Prefer evolving `StoreZip` with a stream writer rather than leaving the DRAM-heavy writer as the backup path.

6. **Content-Length**  
   Prefer **chunked** HTTP (unknown length) so the host need not compute the full ZIP size up front. If the stack requires a length, compute size from metadata + member lengths with a size-only pass that does not retain member payloads (settings length known; bake sizes from FS; history size from snapshot header/file size).

## Risks / Trade-offs

- [Slow client / long stream] → Hold stream state until complete or aborted; free promptly on disconnect; do not pin a finished ZIP.  
- [ZIP bit-3 consumers] → Full restore uses our `StoreZipReader`; verify it accepts data descriptors (extend reader if needed — today it may assume sizes in local headers).  
- [History still large in PSRAM as member source] → Acceptable per Non-Goals; ensure it is not also mirrored into DRAM ZIP staging. Prefer file-backed stream when a durable snapshot exists.  
- [Concurrent Full backup] → Keep single-flight / existing admin gate; streaming state is not re-entrant.

## Migration Plan

- Host-only firmware update; no settings schema migration.
- Older backups remain valid; new backups remain store-method ZIPs readable by current restore once reader supports descriptors (if required).

## Open Questions

- None for planning: buffer band 4–8 KiB and “no whole archive in RAM” are fixed by the request.
