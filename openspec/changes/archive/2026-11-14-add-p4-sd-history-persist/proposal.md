# Proposal

## Why

History today survives reboot only via a one-shot LittleFS snapshot that is deleted after restore, so long-running chart data is lost across power cycles unless an orderly reboot flush ran. On the ESP32-P4 WiFi6 host the board already has a MicroSD slot; when a card is present, history should persist permanently on SD with periodic backups instead of the LittleFS throwaway path.

## What Changes

- **P4 host only**: at boot, attempt to power and mount the onboard MicroSD (SDMMC). Log probe, format, mount success/failure. If the card is present but not formatted, format it and log. The SD filesystem stack MUST support cards **up to 64 GB** (mount pre-formatted media and format blank media appropriately, including exFAT where required for SDXC). If no card / mount fails, keep today’s LittleFS snapshot + delete-after-restore behavior unchanged.
- When SD is available (card present and usable after boot probe):
  - Persist history snapshots to SD using the same binary snapshot format as today’s LittleFS flush.
  - **Do not** delete the snapshot after a successful boot restore.
  - Periodically back up history to SD on a configurable interval (same payload as pre-reboot flush).
  - Still flush to SD on known host restart paths.
  - Stop using LittleFS for history snapshots while the SD path is active.
  - Mount only for probe/restore/save; **after each successful (or completed) SD save, unmount and free mount RAM** so the card is not kept mounted between backups.
- Hardware settings (P4, when SD feature is built): add **interval for backup historical data to SD** — integer minutes, min **5**, max **600**, default **60**. Include in GET/POST `/api/hardware` and global settings export/restore.
- Status **Historical storage** group: when SD persist is active, show **time before save** (countdown to next scheduled SD backup) and the **real size of history data stored on SD** in kibibytes (1024-byte KiB) when a snapshot exists.
- Status new **SD card** group: **status**, **capacity Mb**, **free Mb** (operator and simple-user Status). Hide or show absent/not-applicable on non-P4 / no-SD builds as specified.
- **No remount for statistics**: capacity, free space, and on-SD history size MUST be remembered from mount sessions that already read or write the card (boot probe/restore, periodic/reboot save). Status MUST NOT remount SD solely to refresh those figures.
- Document Waveshare ESP32-P4-WIFI6 SDMMC pin map in `pins.h` / README schematic notes.
- **P4 host only**: set the in-memory log ring target to **512 KiB** (S3 remains 128 KiB).

## Capabilities

### New Capabilities
- `sd-card-storage`: P4 host MicroSD bring-up (power, mount, format-if-needed) for cards up to 64 GB, runtime presence, capacity/free reporting, and logging of SD lifecycle actions.

### Modified Capabilities
- `historical-data`: when SD is available, durable SD snapshots with periodic backup and no delete-after-restore; cached on-SD history size for Status; otherwise keep LittleFS one-shot + delete-after-restore.
- `web-console`: Hardware SD backup interval field; Status Historical storage countdown + on-SD history size kB; Status SD card group; export/restore of the new hardware field.
- `sd-card-storage`: capacity/free (and related) cached only during real I/O mount sessions — no stats-only remount.
- `runtime-heap-allocation`: P4 log ring target **512 KiB**; non-P4 stays **128 KiB**.

## Impact

- Host P4 only (`BOARD_HOST_P4`): SDMMC driver enable, remove or narrow `-DDISABLE_SD` for P4, pin defines, boot mount/format helper, Status/hardware APIs and UI (`data/index.html`, `data/user.html` for Status groups), log ring target 512 KiB.
- `HistoryStore` persist/restore backends: LittleFS vs SD path selection; periodic timer; Status fields for countdown.
- `SettingsManager` / `/api/hardware` / settings export-restore for the new interval.
- S3 host and slave: no SD feature; slave firmware untouched.
- Not in fragile zones `slave-spi-ota` / `firmware-update-process` / `smtp-send` except continuing to call the existing pre-restart history flush hook (target may become SD).
