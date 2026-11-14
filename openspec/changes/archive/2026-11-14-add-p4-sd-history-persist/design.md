# Design

## Context

See proposal.md for motivation. Today `HistoryStore` builds a binary snapshot and writes `/history/snapshot.bin` on LittleFS via `persistSnapshot()`; `restoreSnapshot()` applies it then `discardSnapshotFile()`. Pre-restart flush is hooked through `SettingsManager::setPreRestartHandler` in `main.cpp`.

P4 product host is the Waveshare **ESP32-P4-WIFI6** class board: onboard MicroSD is **4-bit SDMMC**, not the slave SPI bus. Pin map (Zephyr / Waveshare / ESP-IDF board docs):

| Signal | GPIO |
|--------|------|
| CLK | 43 |
| CMD | 44 |
| D0 | 39 |
| D1 | 40 |
| D2 | 41 |
| D3 | 42 |
| Card power enable (active LOW) | 45 |

These do not collide with slave SPI (28–31) or ESP-Hosted SDIO (14–19). `platformio.ini` currently sets `-DDISABLE_SD` on P4 (ESP Mail Client); SD bring-up must enable the SD/SDMMC stack for P4 without breaking SMTP’s “no SD FS hooks” intent (keep mail client SD disabled if still required; use ESP-IDF/Arduino SDMMC APIs directly for history).

## Goals / Non-Goals

**Goals:**
- Boot-time SD probe → mount or format+mount → logged outcomes, then unmount.
- SD filesystem usable with cards **up to 64 GB** (FAT32 + exFAT as needed for SDXC).
- Mount only for probe/restore/save; **unmount and free mount RAM after each save** (and after boot restore/probe session).
- Single “active backend” for history snapshots: SD when a usable card was accepted at boot, else LittleFS.
- Periodic SD backup + reboot flush to SD; no delete-after-restore on SD.
- Hardware interval + Status countdown + Status SD card group on operator and simple-user Status.
- Pins documented in `pins.h` / README.

**Non-Goals:**
- S3 host SD support.
- Hot-plug remount loops beyond boot probe (optional later).
- Changing snapshot binary format or PSRAM slab/query semantics.
- Storing live samples directly on SD (PSRAM remains the live store).
- Sharing SPI with the slave link.

## Decisions

1. **Transport: SDMMC 4-bit, not SDSPI**  
   Match the onboard slot wiring. Prefer Arduino `SD_MMC` / IDF SDMMC host with the pin table above; drive GPIO45 LOW before init.  
   *Alternative considered:* SDSPI on free GPIOs — rejected; board slot is SDMMC.

2. **Filesystem: FAT32 + exFAT (up to 64 GB)**  
   Enable FatFs/exFAT (or Arduino SDMMC equivalent) so pre-formatted SDHC/SDXC cards up to **64 GB** mount. When auto-formatting a blank card, choose a layout valid for that capacity (exFAT for typical ≥32 GB SDXC; FAT32 acceptable where the stack and card size allow). Log before/after format. Cards **> 64 GB** are non-goals.  
   *Alternative considered:* FAT32-only — rejected; native 64 GB SDXC media is commonly exFAT.

3. **Backend selection**  
   After boot SD probe accepts a usable card, set `historyPersistOnSd` (card available), then **unmount**. `HistoryStore` write/read paths branch on that flag; each SD I/O session remounts, uses path e.g. `/sdcard/history/snapshot.bin`, then unmounts. Reuse `buildSnapshot` / `applySnapshot` unchanged.

4. **Delete-after-restore**  
   Call `discardSnapshotFile()` only for LittleFS success path. SD restore success skips delete; then unmount.

5. **Periodic backup + free mount RAM**  
   Host loop/timer: when `historyPersistOnSd` and elapsed ≥ `historySdBackupIntervalMin`, mount → persist (same helper as reboot) → **unmount and free mount/FatFs buffers**. Reset countdown after success; on failure still unmount, log, retry next cycle. Do not leave SD mounted between intervals. Cache last capacity/free **and real history snapshot file size (bytes→KiB)** at end of each successful mount that reads or writes the snapshot.

5b. **SD write commit (3 steps)**  
   Every SD history save: (1) write all data to temp files and verify success; (2) only then remove old normal files; (3) rename temp → normal. Never delete the previous normal snapshot until every temp write succeeded. On rename failure after remove, leave the temp file on the card.

5c. **P4 log ring 512 KiB**  
   P4 host `Logger::kRingCapacityTarget` is **512 KiB** (S3 and other hosts stay **128 KiB**). P4 has enough PSRAM for SD history + a larger diagnostic ring.

6. **Pre-restart flush**  
   Same mount → write → unmount sequence when SD path is active (unmount before `ESP.restart()` if time allows; if restart is immediate after write, still tear down mount so buffers are not left as if permanently mounted across a soft path that continues).

7. **`-DDISABLE_SD` on P4**  
   Keep the mail-client compile flag if it only strips mail SD hooks; if it globally blocks SDMMC, replace with a mail-specific define or remove for P4 and ensure mail still builds without SD. S3 may keep `-DDISABLE_SD`.

8. **Settings**  
   Store interval in hardware reserved/settings alongside other history fields (new key `historySdBackupIntervalMin`). Export/restore with hardware slice. UI label: interval for backup historical data to SD. Show Hardware field on P4 builds only.

9. **Status API**  
   Extend status JSON: `historySdSecondsToSave` (or minutes) when SD persist active; `historySdSizeKiB` (real on-card snapshot size, cached); `sdStatus` (absent / ready / error — not “always mounted”), `sdCapacityMb`, `sdFreeMb` from last mount cache. Render **Historical storage** countdown + on-SD size and **SD card** group in `data/index.html` and `data/user.html`. **Never remount** for Status polls or free-space/size refresh alone.

9b. **Stats cache policy**  
   Update capacity/free/history-file-size only inside existing probe/restore/save mount sessions (stat the snapshot file while mounted). If no snapshot yet, cache size 0.

10. **Logging**  
    Explicit LOGGER lines for: power on, card detect/init, mount ok/fail, format start/result, unmount/free, remount for save, history SD restore keep-file, periodic backup ok/fail.

## Risks / Trade-offs

- **[Risk] Large snapshot write on SD every N minutes** → Mitigation: same packed snapshot as reboot (not per-sample); interval min 5; log duration; avoid holding SPI slave bus (SDMMC is separate).
- **[Risk] Format wipes user card** → Mitigation: format only when mount fails for unformatted/unusable FS; log clearly.
- **[Risk] 64 GB / exFAT not enabled in sdkconfig** → Mitigation: turn on exFAT (and related FatFs options) for P4 host build; verify mount of a 64 GB card and blank-card format in tasks.
- **[Risk] GPIO45 / LDO / 1.8 V UHS quirks** → Mitigation: follow Waveshare/ESP-IDF board defaults for this slot; document if internal LDO config is required.
- **[Risk] `-DDISABLE_SD` conflicts with mail client** → Mitigation: isolate mail disable from SDMMC history path during apply.
- **[Risk] Dual leftover LittleFS + SD snapshots** → Mitigation: when SD path active, ignore LittleFS history snapshot for restore (prefer SD); optional one-time cleanup log.
- **[Risk] Remount cost every backup** → Mitigation: interval min 5 minutes; accept remount latency vs holding FatFs/SDMMC RAM continuously.
- **[Risk] Status capacity/size stale while unmounted** → Mitigation: refresh cache on each real I/O mount; label status as ready/available, not mounted; accept staleness until next save/restore rather than remounting for stats.

## Migration Plan

- First P4 boot with empty/no card: behavior identical to today (LittleFS).
- First boot with card: mount/format, restore from SD if file exists (keep file), start interval timer.
- Rollback to older firmware: leftover SD files ignored; LittleFS path returns.
- Host build bumped to **P4-4.2.58** / **S3-4.2.58** at propose; slave unchanged.

## Open Questions

None blocking. Pin map taken from Waveshare ESP32-P4-WIFI6 public board docs (CLK 43, CMD 44, D0–D3 39–42, power 45).
