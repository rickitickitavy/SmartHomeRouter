## Context

`HistoryStore` already keeps a fixed PSRAM slab of 4096-byte pages plus per-series entry blobs sized by Hardware Entry size. Live sampling, sweep, and chart query stay as they are. Today `begin()` always starts empty after reboot (`historical-data` “PSRAM only”). Operators lose chart continuity across Maintenance reboot and firmware update host restart. Config/settings on LittleFS/EEPROM are more valuable than history charts.

Joined OTA today embeds a single `version.txt` (used mainly as the expected **slave** version) while both chips compile the same `FIRMWARE_VERSION`. Host-only fixes still bump the shared string and often reflash the slave.

## Goals / Non-Goals

**Goals:**
- Snapshot filled entries + used data pages to LittleFS before known host restart paths.
- Best-effort snapshot when unexpected reset paths still run app code.
- Restore at boot into the current slab/entry-size settings; shrink by dropping oldest pages / excess entry slots when capacity shrank.
- Delete snapshot after successful restore.
- Prefer config durability and LittleFS headroom over completing a history snapshot.
- Independent `FIRMWARE_VERSION_HOST` and `FIRMWARE_VERSION_SLAVE` (seed both from the current shared version).
- Joined package carries both versions; skip slave SPI and/or host apply when that chip’s running version already equals the package version for that chip.
- Cursor/agent rule: bump slave build only when slave sources changed.

**Non-Goals:**
- Persisting history continuously during normal operation (no periodic flash wear for every sample).
- Slave-side history or SPI dump of history.
- Surviving hard power cuts with zero CPU time (impossible without always-on flush).
- Changing chart UI, query API shape, or sampling/deadband rules beyond version display.
- Changing SPI begin/chunk/end framing when a transfer **does** run.

## Decisions

### History

1. **Snapshot timing (planned)**  
   Single host flush helper invoked immediately before restart on every known host path that still runs app code: at least `SettingsManager::handlePendingRestart` (covers `/api/reboot` and `consumeHostRestart`), and any other host `ESP.restart()` call sites that are reachable with a live `HistoryStore`. Keep the helper idempotent so double-calls are safe.

2. **Unexpected reset**  
   Best-effort only where a handler already runs (do not invent new brownout ISRs unless one already exists and is safe). Same helper; shorter timeout / skip if LittleFS busy or free space below config headroom.

3. **Config more valuable**  
   Before writing, check LittleFS free space against a reserved floor for settings/config growth. Never overwrite settings/device/user/console/automation/camera/scene files for history. On ENOSPC or write failure: log, skip snapshot, continue restart.

4. **On-disk format**  
   Versioned binary (or tightly framed) blob under a dedicated LittleFS path (e.g. `/history/snapshot.bin` or split header + pages). Store: format version, slab page size (4096), entry size used when saved, page count used, series count, then each series key + entry bytes + each referenced page’s raw 4096 bytes (only allocated/used pages). Omit free slab slots.

5. **Boot order**  
   Mount LittleFS → load settings (history KiB + entry size) → `HistoryStore::begin(current sizes)` → if snapshot present, `restoreFromLittleFs()` with shrink → delete snapshot → `syncSeriesFromDevices` as today (reconcile enabled series; keep restored samples for keys that remain historized).

6. **Shrink rules**  
   - New slab pages < snapshot pages used: drop oldest pages globally / per series (prefer per-series oldest first, then continue until fit) until used pages ≤ new slab.  
   - New entry size < saved: recompute max page records; keep newest end of the ring up to the new max; free pages for dropped records.  
   - If current entry size **increased**, keep layout and leave extra directory slots empty (no need to expand saved bytes).

7. **Delete after load**  
   Remove snapshot only after restore reports success into PSRAM. Corrupt header → delete and start empty.

### Dual versions + joined OTA

8. **Macros**  
   In `Defines.h`: `FIRMWARE_VERSION_HOST` and `FIRMWARE_VERSION_SLAVE`, both initialized to the current shared value (`4.2.02` at plan time). Host binary compiles with host macro as its identity; slave with slave macro. Keep a transitional `FIRMWARE_VERSION` alias equal to the **host** string only where call sites are host-only and not yet migrated; slave must not report the host string.

9. **Joined ZIP members**  
   Require `host.version.txt` and `slave.version.txt` (one line each, trimmed). Join script writes both from the two macros. Accept legacy single `version.txt` as **slave** version only when `slave.version.txt` is absent (migration). Host expected version comes from `host.version.txt` (required to skip/apply host correctly once dual versions ship).

10. **Skip when equal**  
    - Before slave SPI begin: if package has slave image and running slave version equals package slave version → skip entire slave transfer/verify cycle; proceed to host decision.  
    - Before host apply: if package has host image and running host version equals package host version → skip host apply (and host restart if nothing else requires it).  
    - If both skipped → report success / no-op update without programming either chip.  
    - When versions differ, keep existing slave-then-host order, verify, and retry rules for the chip that is actually updating.

11. **Agent bump rule**  
    Update `all-projects` rules/skills (repo + `~/.cursor` twins): on source edits, bump `FIRMWARE_VERSION_HOST` build by 1 when host/shared code changed; bump `FIRMWARE_VERSION_SLAVE` build by 1 **only** if slave-side sources changed in that request; leave slave intact otherwise. `/opsx-propose` bumps host build (and slave only if the proposal’s planned implementation touches slave).

12. **Fragile zones**  
    Implementing OTA skip/dual-version touches `firmware-update-process` and possibly `slave-spi-ota`. Apply MUST stop for explicit zone approval before editing those paths.

## Risks / Trade-offs

- **Flash wear (history)** — mitigated by write-on-restart only, not per sample.  
- **Restart delay** — large history may take hundreds of ms; acceptable before planned reboot; cap or skip on unexpected paths if time-critical.  
- **Partial write** — use temp file + rename or trailing checksum; failed/incomplete → discard on boot.  
- **Spec conflict (history)** — replaces “empty after reboot”; Purpose must allow LittleFS snapshot.  
- **Skip vs verify** — skipping when strings match assumes version strings are trustworthy; keep existing post-transfer verify when a transfer runs.  
- **Legacy packages** — packages with only `version.txt` still drive slave expectation; host skip may use compiled host identity vs absent host.version carefully (prefer requiring `host.version.txt` for new joins).

## Migration Plan

- History: first boot after upgrade with no snapshot → empty; after first planned reboot with feature → restore works.  
- Versions: first dual build ships both macros equal to today’s string; join emits both version files. Old ZIPs with only `version.txt` remain readable as slave version.  
- Rollback: older firmware ignores unknown ZIP members; leftover history snapshot ignored/deleted.

## Open Questions

None blocking. Assumptions: host-only history; config headroom beats history; hard power-off without runtime need not persist; equality is exact string match after trim.
