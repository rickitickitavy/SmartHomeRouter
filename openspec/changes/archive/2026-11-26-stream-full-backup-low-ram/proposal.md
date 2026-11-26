# Proposal

## Why

Full backup currently builds the entire ZIP in memory (DRAM copies of every member, then a finished archive buffer held for the HTTP download). That drops Free RAM by tens of kilobytes (observed ~135 kB → ~66 kB) and is not acceptable on the host.

## What Changes

- Stream Full backup ZIP over HTTP using a small fixed work buffer (**4–8 KiB**), not a whole-archive RAM buffer.
- Stop `StoreZipWriter`/`fullBackupBuildZip` from duplicating every member into internal DRAM and holding the finished ZIP for the response.
- Keep Full backup ZIP contents and operator options the same (settings, optional Wi‑Fi/secrets/history, scene bake JPEGs, manifest).
- Ordinary Export settings JSON download unchanged.
- Full restore upload path unchanged in this change (still accepts a ZIP body as today).
- Cap Camera Resolve/Test (`camOnvif`) FreeRTOS **Free RAM** stack: **16 KiB** for HTTP URLs, **32 KiB** when any request URL is HTTPS (was a flat 64 KiB).

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: Full backup MUST stream the ZIP with a bounded work buffer and MUST NOT keep the complete archive in RAM for the download. Camera Resolve/Test MUST use at most 16 KiB Free RAM stack for HTTP and 32 KiB when HTTPS is required.
- `historical-data`: When history is included in Full backup, packing MUST NOT force a second full-archive DRAM copy; history bytes may be streamed into the ZIP (PSRAM/on-disk snapshot sources OK as member sources).

## Impact

- Host: `FullBackup.cpp` / `StoreZip.cpp` / `WebConsole::handleFullBackupPost` response path; `WebConsole::queueCameraHeavyOp` stack sizing; possibly small helpers to stream bake/history members.
- Status Free RAM during/after Full backup should stay near baseline (work buffer only, plus any pre-existing PSRAM for settings/history sources — not a DRAM-held finished ZIP).
- Camera Resolve/Test Free RAM peak from the worker stack drops from 64 KiB to 16 KiB (HTTP) or 32 KiB (HTTPS).
- Host firmware build: propose **4.6.40**; apply/stream **4.6.41**; camera stack follow-up **4.6.42**.
- No slave changes.
