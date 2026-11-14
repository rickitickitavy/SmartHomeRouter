# Tasks

## 1. P4 SDMMC bring-up

- [x] 1.1 Add P4 SDMMC pin defines (CLK 43, CMD 44, D0–D3 39–42, power 45 active-low) in `include/pins.h` and document them in README schematic notes; verify pins do not overlap slave SPI 28–31 or Hosted 14–19
- [x] 1.2 Enable SDMMC on `esp32-p4-host` only (resolve `-DDISABLE_SD` vs ESP Mail Client so SMTP still builds without claiming the card) and enable FatFs/exFAT (or equivalent) so cards up to **64 GB** mount; verify P4 build succeeds and S3 keeps SD disabled
- [x] 1.3 Implement boot SD power → init → mount, with format-if-unformatted suitable for capacity up to 64 GB, then **unmount and free mount RAM**; LOGGER for absent / mount / format / unmount; verify serial log on boot with no card, blank card, pre-formatted FAT32, and a **64 GB** (or largest available ≤64 GB) card
- [x] 1.4 Provide remount/unmount helpers that release SDMMC/FatFs mount buffers after each session; verify heap (or documented free) recovers after unmount vs while mounted

## 2. History persist backends

- [x] 2.1 Branch `HistoryStore` snapshot present/read/write on active backend (SD path vs LittleFS); when SD usable prefer SD and skip LittleFS history writes; each SD op remounts then unmounts; verify logs show mount→write→unmount
- [x] 2.2 On SD restore success, do **not** delete the SD snapshot; then unmount; keep LittleFS delete-after-restore; verify reboot restores samples, file remains, mount RAM freed
- [x] 2.3 Keep pre-restart flush hooked but targeting the active backend with mount→write→unmount; verify Maintenance reboot writes to SD when usable and to LittleFS when not

## 3. Periodic backup and settings

- [x] 3.1 Add `historySdBackupIntervalMin` (5–600, default 60) to SettingsManager + GET/POST `/api/hardware` (P4) and include it in global settings export/restore + Maintenance hint copy; verify round-trip and 400 on out-of-range
- [x] 3.2 Run periodic SD snapshot on that interval when SD persist is active (mount→save→unmount/free); expose remaining time for Status; verify backup fires near interval, countdown decreases, and card is not left mounted between intervals
- [x] 3.3 Show Hardware field **interval for backup historical data to SD** on P4 only in `data/index.html`; verify field hidden/absent on S3 builds or non-P4 UI path

## 4. Status UI

- [x] 4.1 Extend status API with SD fields (`status` ready/absent/error, cached `capacityMb`/`freeMb`) and history countdown when SD persist active; verify JSON while unmounted between saves still shows last capacity
- [x] 4.2 Add Status **SD card** group (status, capacity Mb, free Mb) and Historical storage **time before save** on both `data/index.html` and `data/user.html`; verify both pages match when SD is usable and hide countdown when SD persist is inactive

## 5. Integration check

- [x] 5.1 On P4: no card → LittleFS history path unchanged; with card → SD path, periodic backup with unmount after save, Status groups populated from cache; confirm host version is P4-4.2.60 (apply bump) and slave version unchanged (4.2.07)
- [x] 5.2 Set P4 `Logger::kRingCapacityTarget` to **512 KiB** (S3 remains 128 KiB); confirm boot log reports 524288-byte ring on P4

## 6. Cached on-SD history size (no stats remount)

- [x] 6.1 During existing SD mount sessions (boot restore/read and save/write), measure real snapshot file size and cache it with capacity/free; verify Status/API never remounts solely for free space or size
- [x] 6.2 Expose cached on-SD history size (KiB) on status API and show it on **Historical storage** in `data/index.html` and `data/user.html` when SD persist is active; verify both pages match and the value updates after a save without an extra mount
