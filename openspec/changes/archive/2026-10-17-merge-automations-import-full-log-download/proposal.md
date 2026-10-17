# Proposal

## Why

1. **Import all** currently replaces the entire automation store, so automations missing from the file are deleted. Operators need to merge imports: keep existing automations that are not in the file, upsert by `name`.
2. **Download full log** is expected to deliver the whole host log ring (PSRAM target is hundreds of KiB), but operators only get about **64 KiB**—the same cap as the on-screen Log viewer. Full download must not be limited to the web-view tail size.

## What Changes

- Automations **Import all**: merge into the store — upsert by `name`, do **not** delete automations absent from the import file. Update confirm copy accordingly.
- **Download full log** (`GET /api/log/download` and the Log tab button): return the entire current ring contents up to `ringCapacityBytes()` (typically the PSRAM ring), not capped at the 64 KiB web-view limit. Fix any client/server truncation that currently yields ~64 KiB. On-screen Log polling may remain capped for UI performance.
- Settings restore that replaces the automations list is unchanged unless it shares the same API path (if shared, add a merge-only path for Import all).

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `automations`: Import all merges (upsert-by-name; keep locals not in file).
- `web-console`: Import all confirm text; Download full log delivers full ring (not 64 KiB view cap).

## Impact

- `AutomationStore` restore/import, `WebConsole` `/api/automations/restore` and `/api/log/download`, `data/index.html` Import all / downloadLog.
- `FIRMWARE_VERSION` build bumped to `3.5.14` at propose time.
- No EEPROM migration. Existing export files remain valid arrays.
