# historical-data Specification

## Purpose

Stores recent device main-state and reportable attribute history in host PSRAM and exposes Home Assistant–like charts so operators and household users can review past values without persisting history to flash.

## Requirements

### Requirement: History lives only in host PSRAM
The host SHALL store historical samples only in PSRAM. History SHALL NOT be written to flash, EEPROM, LittleFS, or the slave. After reboot or power loss the history store SHALL be empty until new samples arrive.

#### Scenario: Reboot clears history
- **WHEN** the host reboots after storing history samples
- **THEN** history query APIs return no samples until new qualifying reports are stored

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
