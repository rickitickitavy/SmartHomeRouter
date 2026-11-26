# Proposal

## Why

Full backup currently builds the entire ZIP in memory (DRAM copies of every member, then a finished archive buffer held for the HTTP download). That drops Free RAM by tens of kilobytes (observed ~135 kB → ~66 kB) and is not acceptable on the host.

## What Changes

- Stream Full backup ZIP over HTTP using a small fixed work buffer (**4–8 KiB**), not a whole-archive RAM buffer.
- Stop `StoreZipWriter`/`fullBackupBuildZip` from duplicating every member into internal DRAM and holding the finished ZIP for the response.
- Keep Full backup ZIP contents and operator options the same (settings, optional Wi‑Fi/secrets/history, scene bake JPEGs, manifest).
- Ordinary Export settings JSON download unchanged.
- Full restore upload path unchanged in this change (still accepts a ZIP body as today).

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: Full backup MUST stream the ZIP with a bounded work buffer and MUST NOT keep the complete archive in RAM for the download.
- `historical-data`: When history is included in Full backup, packing MUST NOT force a second full-archive DRAM copy; history bytes may be streamed into the ZIP (PSRAM/on-disk snapshot sources OK as member sources).

## Impact

- Host: `FullBackup.cpp` / `StoreZip.cpp` / `WebConsole::handleFullBackupPost` response path; possibly small helpers to stream bake/history members.
- Status Free RAM during/after Full backup should stay near baseline (work buffer only, plus any pre-existing PSRAM for settings/history sources — not a DRAM-held finished ZIP).
- Host firmware build bumped to **4.6.40** at propose.
- No slave changes.
