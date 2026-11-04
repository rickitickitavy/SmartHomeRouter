# Tasks

## 1. Version and PSRAM headroom

- [x] 1.1 Confirm `FIRMWARE_VERSION` is `4.0.01` in `include/Defines.h` and verify join/scripts still read that define
- [x] 1.2 Set `Logger::kRingCapacityTarget` to `128 * 1024` and verify host boot with PSRAM reports ring capacity 128 KiB (Status/log download still works)

## 2. Settings model (epsilon + hardware)

- [x] 2.1 Extend `ReportableDataDef` with typed `epsilon` (default `-1`), update JSON parse/emit/copy/seed paths, and verify omitted epsilon loads as `-1` and round-trips in `devices.json`
- [x] 2.2 Add device-level history epsilon (default `-1`) through DeviceTopicMap persist/list/export/import and verify life cycle after reboot
- [x] 2.3 Add Hardware fields Max History store days (1–20, default 7), History sweep interval minutes (1–120, default 10), Maximum history size kB (default **3072**), and Entry size hard choice 256/512/1024/2048/4096 (default 1024); verify GET/POST `/api/hardware` validation and persistence
- [x] 2.4 Update Devices parameter dialog (operator UI): device Hist + epsilon and per-reportable Hist + epsilon (string flag-only; gray/disable epsilon when Hist off; square-only checkbox) and verify load/save matches epsilon rules

## 3. History store core

- [x] 3.1 Implement PSRAM page slab sized from Maximum history size kB (`floor(kB*1024/4096)` pages of 4096), free-list acquire/release, and verify no internal-RAM fallback when slab alloc fails
- [x] 3.2 Implement per-series entry metadata sized by Hardware Entry size (oldest/newest indices, page records; max pages `(EntrySize-12)/12`) in PSRAM and verify boot allocates entry + first page for each enabled series after settings load
- [x] 3.3 Implement page header + fixed records (2010-epoch uint32 datetime + blockLength value) and verify append updates `recordsCount` and page/entry lastDate
- [x] 3.4 Hook live report paths (reportable + per-channel main state / covering position) to change-only+deadband append; verify float within epsilon is skipped and beyond is stored; skip when clock is not sane
- [x] 3.5 Implement age sweep on Hardware interval and allocate-time reclaim (outdated first, then oldest); verify purged pages return to the slab and entry oldest index/startDate update
- [x] 3.6 Expose history counters (entries, pages, slab KiB used, values stored) on `/api/status` and verify Status JSON fields

## 4. History API and charts

- [x] 4.1 Add authenticated history query API (series id + time range + maxPoints) returning ordered samples; verify cap/downsample behavior on a large series
- [x] 4.2 Build HA-like history UI (step line for numeric, colored state bands for binary/state) shared by operator and simple-user pages; verify both pages render the same series from the API
- [x] 4.3 Wire binding name/gauge click in Place and simple-user console view to open that binding’s series only; verify multi-reportable devices open the clicked binding’s series
- [x] 4.4 Add Status **Historical storage** group on operator and simple-user Status (after Free memory) and verify values track the status API

## 5. Hardware UI and integration check

- [x] 5.1 Add the four history fields (including Entry size select) to System → Hardware UI with Save/Reset and verify round-trip plus rejection of non-enum Entry size
- [ ] 5.2 Integration: enable device + reportable history, generate changes, confirm chart, reboot clears samples, sweep removes outdated pages; verify log ring remains 128 KiB under PSRAM pressure with history slab allocated

## Workflow follow-up

- Archive the change after review when implementation is complete.
- Sync main specs via archive or `/opsx-sync` as the project workflow requires.
