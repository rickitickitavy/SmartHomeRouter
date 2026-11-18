# Spec Delta: historical-data

## ADDED Requirements

### Requirement: Flash persist auto-off when SD card present
On boot (and whenever the host detects a usable SD card that activates the SD history path), if S3 persist historical on internal flash is ON, the host SHALL set persist to OFF, save that setting, and SHALL NOT keep flash persist active while SD is present. The console SHALL hide the persist-historical-on-internal-flash control while SD is present.

#### Scenario: Boot with SD disables flash persist
- **WHEN** an S3 host boots with a usable SD card present and flash persist was previously ON
- **THEN** flash persist is OFF after boot reconcile and Status/Internal DB do not treat flash persist as active

#### Scenario: Control hidden with SD
- **WHEN** an admin opens System → Internal DB on an S3 host with SD present
- **THEN** persist historical on internal flash is not shown

## MODIFIED Requirements

### Requirement: Entry size configures page directory capacity
Each series entry SHALL be exactly the configured Internal DB Entry size bytes (256, 512, 1024, 2048, or 4096). Max page records per entry SHALL be `(EntrySize - 12) / 12`.

#### Scenario: Entry size bounds page ring
- **WHEN** Entry size is 256
- **THEN** that series’ page directory holds at most 20 page records

### Requirement: Periodic SD history backup
When the SD history persist path is active, the host SHALL write a full history snapshot to SD on the configured Internal DB interval (minutes). The payload SHALL match the pre-reboot snapshot. Each save SHALL mount (if needed), write with the 3-step commit (temp → remove old → rename), then unmount and free mount RAM. The interval SHALL be at least 5 and at most 600 minutes; factory default SHALL be 60. Changing the interval SHALL apply without requiring a reboot for the next countdown cycle.

#### Scenario: Interval backup runs
- **WHEN** SD persist is active, the interval is 60, and 60 minutes elapse since the last successful SD snapshot (or since boot if none yet)
- **THEN** the host writes a history snapshot to SD and then unmounts, freeing mount RAM

#### Scenario: Interval bounds
- **WHEN** an admin saves an SD backup interval of 4 or 601
- **THEN** the host rejects the value and keeps the previous interval

### Requirement: Status exposes time before next SD save
When the SD history persist path is active, the gateway status API SHALL expose the remaining time until the next scheduled SD history backup so Status can show **time before save**. When SD persist is not active, that field SHALL be omitted or null and Status SHALL NOT show a misleading countdown.

#### Scenario: Countdown while SD active
- **WHEN** SD persist is active and 10 minutes remain before the next backup
- **THEN** the status API reports that remaining time for Internal database

### Requirement: Status exposes real on-SD history size
When the SD history persist path is active and a history snapshot file exists on the card, the gateway status API SHALL expose the **real size** of that stored history data in kibibytes (1024-byte KiB), based on the actual file size(s) on SD (not the in-PSRAM slab usage). The host SHALL capture and remember that size when it reads or writes the snapshot during an existing mount session (boot restore/read or save). Status MUST NOT remount SD solely to measure that size. When no snapshot is on the card yet, the size SHALL be zero or omitted as specified for the UI. Status SHALL label this figure **on-SD size** under Internal database.

#### Scenario: Size after save
- **WHEN** SD persist is active and a successful save wrote a 200 KiB snapshot file
- **THEN** Internal database can show on-SD size 200 kB from cache without another mount

#### Scenario: Size after boot restore
- **WHEN** boot restores from an existing SD snapshot during the restore mount session
- **THEN** the cached on-SD history size reflects that file’s real size for later Status polls

### Requirement: S3 persist historical on internal flash
On the ESP32-S3 host, Internal DB settings SHALL include **persist historical on internal flash** (boolean; factory default **true** when no SD card is present). Persist MAY be ON only when **Maximum history size** is ≤ **4096 KiB** and no usable SD card is present. When size is set above 4096 KiB, or when a usable SD card is present, the host SHALL automatically set persist to **false** and save that with history/hardware settings. The control SHALL be available for Main and Repeater roles when shown (not role-hidden solely for Repeater).

#### Scenario: Default persist on S3
- **WHEN** an S3 host loads settings with no prior persist flag stored, Maximum history size is ≤ 4096 KiB, and no SD card is present
- **THEN** persist historical on internal flash is true

#### Scenario: Default false when size already over gate
- **WHEN** an S3 host loads settings with no prior persist flag and Maximum history size is greater than 4096 KiB
- **THEN** persist historical on internal flash is false

#### Scenario: Size gate blocks ON
- **WHEN** Maximum history size is 8192 KiB and the operator attempts to enable persist on internal flash
- **THEN** the host rejects or forces persist off so persist remains false

#### Scenario: Raising size auto-turns persist OFF
- **WHEN** persist is ON and Maximum history size is changed to a value greater than 4096 KiB
- **THEN** the host sets persist to false and saves settings accordingly

#### Scenario: SD present forces persist OFF
- **WHEN** a usable SD card is present at boot
- **THEN** persist historical on internal flash is false

### Requirement: S3 flash persist periodic save interval
When S3 persist historical on internal flash is ON, the host SHALL write a full history snapshot to LittleFS on the configured Internal DB interval (minutes). The payload SHALL match the pre-reboot snapshot format. The interval SHALL be at least **30** and at most **600** minutes; factory default SHALL be **120**. Changing the interval SHALL apply without requiring a reboot for the next countdown cycle. When persist is OFF, this periodic LittleFS interval save SHALL NOT run.

#### Scenario: Default interval
- **WHEN** persist is ON and the flash backup interval was never saved
- **THEN** the effective interval is 120 minutes

#### Scenario: Interval elapsed
- **WHEN** persist is ON, the interval is 120, and 120 minutes elapse since the last successful LittleFS snapshot (or since boot if none yet)
- **THEN** the host writes a full history snapshot to LittleFS

#### Scenario: Persist off skips periodic save
- **WHEN** persist is OFF
- **THEN** the host does not run the flash persist interval save cycle

### Requirement: Status exposes real on-flash history size
When S3 persist historical on internal flash is ON, the gateway status API SHALL expose the **real** LittleFS history snapshot size in KiB (file size on flash, not PSRAM slab). The host SHALL cache that size during existing restore/save sessions. Status MUST NOT open LittleFS solely to measure size. Missing snapshot → zero or omitted; persist OFF → omitted or null. Status SHALL show this under **Internal database**.

#### Scenario: Cached size after save
- **WHEN** persist is ON and a successful save wrote a 200 KiB LittleFS snapshot
- **THEN** Internal database can show On-flash history size 200 kB from cache without another filesystem open solely for size

#### Scenario: Cached size after boot restore
- **WHEN** persist is ON and boot restore reads a snapshot file of known length
- **THEN** the cached on-flash history size reflects that file’s real size for later Status polls
