## 1. Snapshot format and HistoryStore API

- [x] 1.1 Add versioned LittleFS snapshot save/load/delete APIs on `HistoryStore` (filled entries + used 4096-byte pages only; checksum/integrity)
- [x] 1.2 Implement restore-with-shrink for smaller Maximum history size (drop oldest pages until slab fits) and smaller Entry size (trim page directory, free orphaned pages)
- [x] 1.3 Enforce config-safe LittleFS free-space check; skip snapshot rather than risk settings/config; never overwrite other store files
- [x] 1.4 Unit-test or focused native tests for shrink (smaller KiB, smaller entry size) and corrupt-snapshot discard

## 2. Boot restore and cleanup

- [x] 2.1 After `historyStore.begin` with current settings sizes, load snapshot if present; on success delete it; on failure/corrupt delete and leave empty
- [x] 2.2 Keep `syncSeriesFromDevices` after restore so disabled series can drop and enabled series stay consistent
- [ ] 2.3 Verify reboot with snapshot restored returns samples from history query; cold boot without snapshot stays empty

## 3. Flush on every known host restart path

- [x] 3.1 Invoke snapshot flush immediately before restart in `SettingsManager::handlePendingRestart` (covers Maintenance `/api/reboot` and host OTA `consumeHostRestart` → `requestRestart`)
- [x] 3.2 Audit other host `ESP.restart()` / restart-request call sites; call the same flush helper wherever application code still runs
- [x] 3.3 Best-effort flush on unexpected host reset paths that already run app handlers, without long blocking or endangering config
- [x] 3.4 Confirm slave-only restart paths do not attempt host history I/O

## 4. Split host/slave firmware versions

- [x] 4.1 Replace shared `FIRMWARE_VERSION` with `FIRMWARE_VERSION_HOST` and `FIRMWARE_VERSION_SLAVE` in `Defines.h`, both seeded to the current version string; wire host/slave builds to report the matching macro
- [x] 4.2 Update Status / version JSON so host and slave versions are distinct
- [x] 4.3 Update `firmware_join.py` / split helpers and `JoinedFirmwareZip` to embed and read `host.version.txt` and `slave.version.txt` (legacy `version.txt` → slave only)
- [x] 4.4 In joined OTA (after fragile-zone approval): before slave SPI, skip when running slave version equals package slave version; before host apply, skip when running host version equals package host version; both equal → no-op success
- [x] 4.5 Keep existing slave verify/retry behavior only when a slave transfer actually runs
- [x] 4.6 Update agent bump rules/skills (repo + `~/.cursor` twins): bump host build on host/shared source changes; bump slave build **only** when slave sources changed

## 5. Spec / ops check

- [ ] 5.1 Confirm status Historical storage counts survive a Maintenance reboot when a snapshot was written
- [ ] 5.2 Confirm reducing Max history size / Entry size in Hardware, then rebooting with an older larger snapshot, shrinks rather than failing boot
- [ ] 5.3 Confirm joined update with matching slave version skips SPI and still updates host when host versions differ (and the reverse)
- [ ] 5.4 Confirm both versions matching yields no-op without programming either chip
