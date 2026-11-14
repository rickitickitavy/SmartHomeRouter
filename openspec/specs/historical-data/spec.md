# historical-data Specification

## Purpose

Stores recent device main-state and reportable attribute history in host PSRAM, optionally snapshots filled pages to LittleFS across host restarts, and exposes Home Assistant–like charts so operators and household users can review past values.

## Requirements

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

### Requirement: Flush history before known host restarts
Before every known host restart path that still runs application code—including Maintenance `/api/reboot` via `SettingsManager` pending restart, host restart after joined firmware apply, and any other host `ESP.restart()` / restart-request path in host firmware—the host SHALL attempt to flush all filled history entries and data pages from PSRAM to the **active** history snapshot store (LittleFS when SD persist is inactive; SD when SD persist is active). Empty series and free slab slots NEED NOT be written. Failure to snapshot MUST NOT block the restart indefinitely; the host MAY log and proceed.

#### Scenario: Maintenance reboot flushes
- **WHEN** an admin posts `/api/reboot` and the pending restart is about to call `ESP.restart()`
- **THEN** the host attempts a history snapshot to the active store before restarting

#### Scenario: Firmware host restart flushes
- **WHEN** host firmware apply schedules a host restart through the existing restart-request path
- **THEN** the host attempts a history snapshot before that restart

#### Scenario: SD path flush on reboot
- **WHEN** SD persist is active and a known host restart path runs with history samples
- **THEN** the host attempts the snapshot to SD (not LittleFS)

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
After the host successfully restores a **LittleFS** snapshot into PSRAM (including after any required shrink), it SHALL delete the LittleFS snapshot so a later boot does not reload stale data. If restore fails, the host MAY delete a corrupt LittleFS snapshot; it MUST NOT leave a half-applied PSRAM store paired with an unread LittleFS snapshot that would double-apply on the next boot. When the **SD** history persist path is active, the host SHALL restore from SD without deleting the SD snapshot after success (see SD restore requirement).

#### Scenario: Snapshot removed after load
- **WHEN** boot restores a valid **LittleFS** snapshot into PSRAM
- **THEN** the LittleFS snapshot file(s) are removed before normal operation continues

#### Scenario: SD snapshot kept after load
- **WHEN** SD persist is active and boot restores a valid SD snapshot into PSRAM
- **THEN** the SD snapshot file is not removed solely because restore succeeded

### Requirement: History uses a fixed page slab
The host SHALL reserve a contiguous PSRAM slab of 4096-byte pages whose total size is the configured Maximum history size in kibibytes (1024-byte KiB). History data pages and free slots SHALL come only from that slab. The host SHALL NOT return history pages to the general SPIRAM heap allocator.

#### Scenario: Page comes from slab
- **WHEN** a series needs a new history page and the slab has a free slot
- **THEN** the host assigns a 4096-byte slot from the slab without a general-heap 4096-byte malloc for that page

### Requirement: Series entry metadata and first page at boot
For each historized series after settings load (or when history is newly enabled and saved), the host SHALL allocate that series’ entry metadata and its first history data page from PSRAM. Entry metadata SHALL hold overall start/last times, oldest/newest page indices, and per-page start/last times plus slab address.

#### Scenario: Enabled series gets entry and page
- **WHEN** the host finishes loading settings that enable history on one reportable attribute
- **THEN** that series has entry metadata and one allocated empty data page ready for samples

### Requirement: Entry size configures page directory capacity
Each series entry SHALL be exactly the configured Hardware Entry size bytes (256, 512, 1024, 2048, or 4096). Max page records per entry SHALL be `(EntrySize - 12) / 12`.

#### Scenario: Entry size bounds page ring
- **WHEN** Entry size is 256
- **THEN** that series’ page directory holds at most 20 page records

### Requirement: Fixed record layout inside a page
Each history data page SHALL begin with a header that includes recordsCount (uint16), blockLength (uint8 value length without the datetime), and one reserved byte. Each record SHALL be datetime (uint32 seconds since 2010-01-01 00:00:00 UTC) followed by a fixed-length value of blockLength bytes. Strings SHALL pad with 0x00 to blockLength or truncate to blockLength. All records in one page SHALL share the same blockLength.

#### Scenario: Append uses recordsCount
- **WHEN** a page has recordsCount N and blockLength B
- **THEN** the next record is written at header size plus N times (4 + B) bytes and recordsCount becomes N + 1

### Requirement: Sample only on change beyond deadband
When a live value arrives for a historized series, the host SHALL append a sample only if it differs from the last stored value by at least the series epsilon (change-only + deadband). For string series, any inequality versus the last stored string SHALL append (no numeric deadband). For bool or exact discrete state series with epsilon >= 0, any state change SHALL append. The host SHALL NOT append when the clock is not yet a sane wall clock for the 2010-based timestamp.

#### Scenario: Float within deadband ignored
- **WHEN** the last stored float is 20.0, epsilon is 0.5, and a report of 20.3 arrives
- **THEN** the host does not append a new history record

#### Scenario: Float beyond deadband stored
- **WHEN** the last stored float is 20.0, epsilon is 0.5, and a report of 20.6 arrives
- **THEN** the host appends one new record with that value

### Requirement: Device main-state history is per channel
When device-level history is enabled (device epsilon >= 0), the host SHALL store one series per channel for that device’s main state. For window-covering (and similar position actuators) the stored value SHALL be position only. For on/off-class actuators the stored value SHALL be the on/off state per channel.

#### Scenario: Two-channel on/off
- **WHEN** a two-channel on/off device has history enabled and channel 2 turns on
- **THEN** only the channel-2 main-state series receives a new sample

### Requirement: Age sweep removes oldest outdated pages
On the configured History sweep interval, the host SHALL purge history pages whose lastDate is older than Max History store days. Sweep SHALL start at the oldest page of each series. Purging SHALL update the series entry start time and oldest-page index and return the page slot to the slab free list.

#### Scenario: Outdated oldest page freed
- **WHEN** Max History store days is 7 and a series’ oldest page lastDate is more than 7 days before now
- **THEN** that page slot is freed and the series entry no longer references it

### Requirement: New page allocation reclaims when needed
Before allocating a new page for a series, the host SHALL purge outdated pages for that series when any exist. If the slab has no free page or the series page ring is full, the host SHALL purge oldest pages (that series first, then globally as needed) until a slot is available or no reclaimable page remains. On success the host SHALL take a free slab page, write the new sample, and append a page record at the newest end of the series entry.

#### Scenario: Full slab reclaims oldest
- **WHEN** the slab has no free page and at least one oldest page can be purged
- **THEN** the host frees that page and allocates the new page for the incoming sample

### Requirement: History query API for charts
The host SHALL expose an authenticated history query API that returns samples for one series over a time range, suitable for chart rendering, with a bounded maximum point count (downsample or truncate as needed). Numeric series SHALL be chartable as a step-held line; binary/state series SHALL be chartable as colored state bands.

#### Scenario: Range query returns points
- **WHEN** a signed-in user requests history for an enabled series over the last day and samples exist
- **THEN** the response includes ordered timestamped values for that series within the requested range up to the point cap

### Requirement: Binding click opens that series history
When a user activates a console binding (including gauge or name) whose bound parameter maps to a historized series, the console SHALL open the history chart for that bound series. The same behavior SHALL apply on the operator console and the simple-user console.

#### Scenario: Gauge opens reportable history
- **WHEN** a user clicks a gauge bound to a historized temperature reportable
- **THEN** the history UI shows that temperature series, not another attribute of the same device

### Requirement: SD persist path when MicroSD is usable
On the P4 host, when boot finds a usable MicroSD card (probe/mount succeeds, including after format-if-needed), the host SHALL use the SD card as the durable history snapshot store (same snapshot contents as the existing LittleFS flush). While that SD path is active, the host SHALL NOT write history snapshots to LittleFS. “Active” means the card was accepted at boot, not that it stays mounted. When no usable SD is found, the host SHALL keep the existing LittleFS snapshot behavior.

#### Scenario: Usable SD uses SD for snapshots
- **WHEN** P4 boot accepts a usable MicroSD card and history has samples
- **THEN** the next history persist writes the snapshot to SD, not LittleFS

#### Scenario: No SD keeps LittleFS
- **WHEN** P4 boot finds no usable SD (or non-P4 host)
- **THEN** history persist and restore continue to use LittleFS as today

### Requirement: SD history save uses a 3-step commit
When writing history snapshot data to SD, the host SHALL use this commit order and SHALL NOT remove or replace the previous normal files until every temp write has succeeded:

1. Write all snapshot data to temporary files (leave existing normal files untouched).
2. If and only if every temp write succeeded, remove the old normal files.
3. Rename each temporary file to its normal name.

If step 1 fails, the host SHALL leave the previous normal files intact and SHALL NOT proceed to steps 2–3. After the save session (success or failure), the host SHALL unmount and free mount RAM as elsewhere.

#### Scenario: Failed temp write keeps old snapshot
- **WHEN** an SD history save cannot finish writing a temporary file
- **THEN** the previous normal snapshot file remains and is not deleted

#### Scenario: Successful save replaces via rename
- **WHEN** an SD history save writes the temporary snapshot completely
- **THEN** the host removes the old normal file and renames the temporary file to the normal name

### Requirement: Periodic SD history backup
When the SD history persist path is active, the host SHALL write a full history snapshot to SD on the configured Hardware interval (minutes). The payload SHALL match the pre-reboot snapshot. Each save SHALL mount (if needed), write with the 3-step commit (temp → remove old → rename), then unmount and free mount RAM. The interval SHALL be at least 5 and at most 600 minutes; factory default SHALL be 60. Changing the interval SHALL apply without requiring a reboot for the next countdown cycle.

#### Scenario: Interval backup runs
- **WHEN** SD persist is active, the interval is 60, and 60 minutes elapse since the last successful SD snapshot (or since boot if none yet)
- **THEN** the host writes a history snapshot to SD and then unmounts, freeing mount RAM

#### Scenario: Interval bounds
- **WHEN** an admin saves an SD backup interval of 4 or 601
- **THEN** the host rejects the value and keeps the previous interval

### Requirement: SD restore keeps the snapshot file
When the SD history persist path is active and boot restores a valid snapshot from SD into PSRAM, the host SHALL leave that snapshot file on the card (SHALL NOT delete it after successful restore). After the restore session, the host SHALL unmount and free mount RAM. Corrupt SD snapshots MAY still be discarded or ignored so a bad file does not loop forever.

#### Scenario: Boot restore leaves SD file
- **WHEN** SD persist is active and boot successfully restores history from the SD snapshot
- **THEN** the SD snapshot file remains on the card after restore and the filesystem is unmounted afterward

### Requirement: Status exposes time before next SD save
When the SD history persist path is active, the gateway status API SHALL expose the remaining time until the next scheduled SD history backup so Status can show **time before save**. When SD persist is not active, that field SHALL be omitted or null and Status SHALL NOT show a misleading countdown.

#### Scenario: Countdown while SD active
- **WHEN** SD persist is active and 10 minutes remain before the next backup
- **THEN** the status API reports that remaining time for Historical storage

### Requirement: Status exposes real on-SD history size
When the SD history persist path is active and a history snapshot file exists on the card, the gateway status API SHALL expose the **real size** of that stored history data in kibibytes (1024-byte KiB), based on the actual file size(s) on SD (not the in-PSRAM slab usage). The host SHALL capture and remember that size when it reads or writes the snapshot during an existing mount session (boot restore/read or save). Status MUST NOT remount SD solely to measure that size. When no snapshot is on the card yet, the size SHALL be zero or omitted as specified for the UI.

#### Scenario: Size after save
- **WHEN** SD persist is active and a successful save wrote a 200 KiB snapshot file
- **THEN** Historical storage can show on-SD history size 200 kB from cache without another mount

#### Scenario: Size after boot restore
- **WHEN** boot restores from an existing SD snapshot during the restore mount session
- **THEN** the cached on-SD history size reflects that file’s real size for later Status polls
