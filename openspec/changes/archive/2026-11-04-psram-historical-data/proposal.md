# Proposal

## Why

Operators and household users can see live device values but cannot review how those values or actuator states changed over recent days. A host-side PSRAM history store with Home Assistant–like charts gives that view without flash wear, while a fixed page slab avoids SPIRAM fragmentation under competing PSRAM users (logs, cameras, OTA).

## What Changes

- Add host PSRAM historical storage for opted-in device main state (per channel) and opted-in reportable attributes, using fixed 4096-byte pages in a global slab sized by Hardware **Maximum history size (kB)** (default **3072**).
- Persist history enablement via a typed **epsilon** on each reportable row and on each device (replaces a separate Hist boolean): `epsilon < 0` means history off (default `-1`); `epsilon >= 0` means history on with that deadband (strings use `0` = on / `< 0` = off; no deadband). UI shows a Hist flag plus epsilon field (epsilon grayed/disabled when flag off).
- Record a new sample only when the value changes beyond the deadband (change-only + deadband). Charts use step-hold like Home Assistant. Binary/state series render as colored state bands; numeric sensors as line charts.
- Sweep outdated pages by Hardware **Max History store days** (1–20, default 7) on **History sweep interval (minutes)** (1–120, default 10); reclaim slab slots from oldest pages first.
- Hardware **Entry size** is a hard choice among **256, 512, 1024, 2048, 4096** bytes (default **1024**): size of each series’ history entry (page directory). Max pages per series is `(EntrySize - 12) / 12`.
- Status gains a **Historical storage** group: entries count, pages count, PSRAM usage (kB), values stored.
- Clicking a console binding (gauge/name) that maps to a historized series opens that series’ history UI on **both** operator and simple-user pages.
- **BREAKING** (capacity): host log ring target capacity decreases from **512 KiB** to **128 KiB** to free PSRAM for history.
- Firmware version set to **4.0.01**.

## Capabilities

### New Capabilities

- `historical-data`: Host PSRAM page-slab history store, sampling rules, purge/sweep, query API, and Home Assistant–like history charts for device main-state and reportable series.

### Modified Capabilities

- `device-registry`: Device and `reportableData` rows persist history epsilon; full life cycle (save/list/`devices.json`/export/import/UI).
- `web-console`: Status Historical storage; Hardware history settings; device/reportable Hist+epsilon editors; history chart surfaces for allowed users.
- `control-consoles`: Binding click opens history for the bound historized parameter/series on operator and simple-user views.
- `runtime-heap-allocation`: History slab and related metadata stay in PSRAM; log ring target capacity is 128 KiB.

## Impact

- Host only (`esp32-s3`); slave unchanged for history bytes. Samples taken where live reports already update the host cache.
- PSRAM budget: smaller log ring + configurable history slab; Status reflects history usage.
- Device APIs / `devices.json` / reportable JSON gain epsilon fields (default history off).
- New history query HTTP API for charts; console JS/CSS on `data/index.html` and `data/user.html`.
- Hardware settings API and `GlobalSettings` reserved fields for days, sweep interval, max history kB, and entry size.
