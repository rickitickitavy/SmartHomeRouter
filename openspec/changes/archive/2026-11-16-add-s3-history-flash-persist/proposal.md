# Proposal

## Why

ESP32-S3 hosts currently treat LittleFS history snapshots as a one-shot reboot survival path that is deleted after restore, so durable periodic history on internal flash is missing. Operators need an S3 option to keep history across boots without a MicroSD card, with clear size and interval limits so flash wear and free space stay bounded.

## What Changes

- On **S3 host** System → Hardware, add **persist historical on internal flash** (default **true**). The control MAY be turned ON only when **Maximum history size** is **≤ 4096 KiB**. If size is later raised above 4096 KiB while persist is ON, the host SHALL automatically turn persist OFF.
- When persist is ON, expose **interval save historical data** for S3 (same role as P4’s SD backup interval): **min 30**, **max 600**, **default 120** minutes.
- When persist is ON, after boot restore of stored history the host SHALL **not** delete the internal-flash snapshot file(s) (unlike today’s LittleFS delete-after-restore path).
- When persist is ON, Status **Historical storage** SHALL show **On-flash history size** (real LittleFS snapshot size in KiB, same role as P4 **on-SD history size**). The same cached value SHALL also appear under Status **File system** on S3 only as **Historical data size**.
- P4 SD persist behavior stays as today; this feature is S3 / non-SD flash persist.
- Hardware settings (including these fields) remain in global settings export/restore.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `historical-data`: S3 internal-flash persist mode, size gate, periodic save interval, keep snapshot after boot restore when persist is ON; cached on-flash history size for Status.
- `web-console`: Hardware UI/API for S3 persist checkbox and interval field (and auto-off when size exceeds 4096 KiB); Status Historical storage **On-flash history size**; Status File system **Historical data size** (S3 only).

## Impact

- Host: `SettingsManager` / hardware settings, `HistoryStore` LittleFS persist/restore/delete rules, boot sequence, Hardware page and Status (`data/index.html` and `data/user.html`) for S3.
- Status shows time-before-save and on-flash size when S3 flash persist is active (P4 SD parity for the history widget; File system duplicate on S3 only).
- No slave firmware changes.
- Host firmware build bump for this propose.
