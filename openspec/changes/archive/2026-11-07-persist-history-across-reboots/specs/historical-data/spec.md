## MODIFIED Requirements

### Requirement: History lives in host PSRAM with optional LittleFS snapshot
The host SHALL keep the live historical sample store in PSRAM (fixed page slab as today). History SHALL NOT be stored on the slave. The host MAY write a LittleFS snapshot of filled entries and data pages solely to survive host restarts. Settings, config, and other durable stores on LittleFS/EEPROM SHALL take priority over history snapshots: the host MUST NOT displace, truncate, or endanger those stores to make room for history, and MUST skip or shrink a history snapshot when free space or write reliability would put config at risk.

#### Scenario: Planned reboot restores samples
- **WHEN** the host has history samples in PSRAM, a known host restart path runs (including Maintenance reboot and host firmware-update restart), the snapshot write succeeds, and the host boots again with unchanged history size settings
- **THEN** history query APIs return the restored samples after boot

#### Scenario: No snapshot means empty store
- **WHEN** the host boots and no valid history snapshot is present on LittleFS
- **THEN** the history store initializes empty until new samples arrive (same as today’s cold start)

#### Scenario: Config wins over history snapshot
- **WHEN** LittleFS free space is at or below the reserved headroom needed for settings/config durability
- **THEN** the host skips or aborts the history snapshot write and still completes the restart without corrupting config

## ADDED Requirements

### Requirement: Flush history before known host restarts
Before every known host restart path that still runs application code—including Maintenance `/api/reboot` via `SettingsManager` pending restart, host restart after joined firmware apply, and any other host `ESP.restart()` / restart-request path in host firmware—the host SHALL attempt to flush all filled history entries and data pages from PSRAM to the LittleFS snapshot. Empty series and free slab slots NEED NOT be written. Failure to snapshot MUST NOT block the restart indefinitely; the host MAY log and proceed.

#### Scenario: Maintenance reboot flushes
- **WHEN** an admin posts `/api/reboot` and the pending restart is about to call `ESP.restart()`
- **THEN** the host attempts a history snapshot to LittleFS before restarting

#### Scenario: Firmware host restart flushes
- **WHEN** host firmware apply schedules a host restart through the existing restart-request path
- **THEN** the host attempts a history snapshot before that restart

### Requirement: Best-effort flush on unexpected host reset when possible
When the host still has a chance to run flush logic before an unexpected reset (for example brownout/critical paths that still execute application handlers), it MAY attempt the same LittleFS snapshot. That attempt MUST remain secondary to config/settings integrity and MUST NOT introduce long blocking writes that risk watchdog or config loss. Hard power cuts with no CPU run time NEED NOT leave a snapshot.

#### Scenario: Unexpected path with time to flush
- **WHEN** a host fault path still runs application code long enough to write and LittleFS has config-safe free space
- **THEN** the host may write a history snapshot before reset

### Requirement: Boot restore shrinks when capacity decreased
After allocating the PSRAM slab with the **current** Maximum history size (KiB) and Entry size, if a snapshot exists the host SHALL load it into that slab. If the snapshot’s page count or entry layout does not fit the current sizes, the host SHALL shrink: drop the oldest data pages first until pages fit the new slab, and shrink or drop entry directories so each series fits the new Entry size (discarding excess newest-side page records that no longer fit, and throwing away any data pages those discarded records referenced). Corrupt or incompatible snapshots SHALL be discarded and the store SHALL start empty.

#### Scenario: Smaller slab drops oldest pages
- **WHEN** a snapshot was taken with a larger Maximum history size and boot uses a smaller KiB setting so not all pages fit
- **THEN** the host restores only the newest-fitting pages and drops the oldest overflow pages

#### Scenario: Smaller entry size trims page directory
- **WHEN** a snapshot was taken with a larger Entry size and boot uses a smaller Entry size so a series’ page ring no longer fits
- **THEN** the host shrinks that entry to the new max page-record count, drops the overflow (oldest) page records, and releases those pages

### Requirement: Delete snapshot after successful restore
After the host successfully restores a snapshot into PSRAM (including after any required shrink), it SHALL delete the LittleFS snapshot so a later boot does not reload stale data. If restore fails, the host MAY delete a corrupt snapshot; it MUST NOT leave a half-applied PSRAM store paired with an unread snapshot that would double-apply on the next boot.

#### Scenario: Snapshot removed after load
- **WHEN** boot restores a valid snapshot into PSRAM
- **THEN** the LittleFS snapshot file(s) are removed before normal operation continues
