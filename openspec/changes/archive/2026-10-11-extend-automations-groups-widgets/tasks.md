# Tasks

## 1. Automation model and timer edge

- [x] 1.1 Add persisted `group` on `AutomationRecord` (parse/serialize/create/update/list/summary/settings); empty = ungrouped; verify reboot round-trip and name-safe characters.
- [x] 1.2 On `active` FALSE→TRUE, start timer when `use` and `defaultTime > 0` and (`autoRestart` or `startOnBoot`); keep post-boot `startOnBoot` delay; verify both edges and deactivate-stops-timer.

## 2. Automation UI groups

- [x] 2.1 Edit dialog: Group field immediately under Name (text + datalist of existing groups); save/load `group`.
- [x] 2.2 Automation section: narrow left group list (**All**, named groups, **Not in group**) filtering the right-hand table; verify filter and empty-group bucket.

## 3. Console automation widgets

- [x] 3.1 Extend `ConsoleBinding` + JSON for automation targets; reject invalid names on save; old device-only JSON still loads.
- [x] 3.2 Consoles editor: add automation binding; preview On/Off without changing `active` until user view.
- [x] 3.3 `user.html`: render automation rows as On/Off; toggle calls host to set `active`; group On/Off ignores automations; missing automation handled safely.

## 4. Status free memory

- [x] 4.1 Gateway status JSON exposes free heap/PSRAM kB and free percent; Status card labels/values show free (not used); PSRAM absent → em dash.

## 5. Apply version

- [x] 5.1 Confirm `FIRMWARE_VERSION` build was bumped at propose (`1.1.05`); during apply bump again if source changes land; verify `pio run -e esp32-s3-host`.
