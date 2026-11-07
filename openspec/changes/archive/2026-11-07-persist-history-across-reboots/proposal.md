## Why

Historical samples today live only in host PSRAM and are lost on every reboot. Operators expect charts to survive planned restarts (Maintenance reboot, firmware update host reboot, and other known host restart paths) and, when feasible, unexpected resets—without ever risking settings/config durability or LittleFS headroom that config needs.

Host and slave currently share one `FIRMWARE_VERSION`, so a host-only change still forces a slave OTA (and the reverse). Operators need independent host/slave versions in the joined package, skip flashing a chip whose running version already matches the package, and bump the slave build only when slave code actually changed.

## What Changes

### History persistence
- Before every known host restart path, flush filled history entries and data pages from PSRAM to a LittleFS snapshot (best-effort).
- On best-effort unexpected reset paths (when the firmware still has a chance to run shutdown/flush logic), attempt the same snapshot; never block or endanger config/settings writes.
- At boot after settings load: if a snapshot exists, restore into the new PSRAM slab; otherwise initialize empty series as today.
- On restore, honor the **current** Maximum history size and Entry size. If the snapshot does not fit, shrink by dropping the oldest pages first and shrinking/dropping excess entry capacity; discard leftover pages that no longer fit.
- After a successful restore into PSRAM, delete the LittleFS snapshot.
- LittleFS space and config integrity take priority over history: refuse or truncate a history snapshot rather than displace or endanger settings/config files.

### Split host/slave firmware versions
- Replace the single shared version with `FIRMWARE_VERSION_HOST` and `FIRMWARE_VERSION_SLAVE` (initially both set to the current version string).
- Joined ZIP carries **two** version members (host and slave). Join tooling embeds both.
- Before programming slave or host during joined update, compare that chip’s running version to the package’s version for that chip; **skip** the transfer/apply when they are already equal.
- Agent/project rule: bump **host** build on host (or shared) source changes as today; bump **slave** build **only** when slave-side source changed; leave slave intact otherwise.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `historical-data`: Replace “PSRAM only / empty after reboot” with LittleFS snapshot flush/restore, shrink-on-load when capacity shrank, and delete-after-load. Config/settings remain more valuable than history when space or reliability conflict.
- `joined-firmware-ota`: Dual version members in the joined package; per-chip skip when running version already matches; host/slave version macros replace the single shared string for OTA expectations and Status.

## Impact

- Host `HistoryStore` (+ boot in `main.cpp`): snapshot save/load/delete; shrink logic when entry size or slab KiB decreased.
- Host restart orchestration: flush before `SettingsManager` scheduled restart, host firmware-apply restart, and any other host `ESP.restart()` / restart-request path that still runs application code.
- LittleFS: new history snapshot artifact(s); must not starve config/stores.
- `Defines.h`, join/split scripts, `JoinedFirmwareZip`, `FirmwareOta` (fragile zone `firmware-update-process` / `slave-spi-ota` — apply needs explicit zone approval), Status Version UI, and agent bump rules (`all-projects` + twins).
- Spec `historical-data` Purpose and the PSRAM-only requirement change accordingly.
- Status/history UI: samples may survive reboot; Version group shows distinct host and slave versions.
