## ADDED Requirements

### Requirement: S3 persist historical on internal flash
On the ESP32-S3 host, Hardware settings SHALL include **persist historical on internal flash** (boolean; factory default **true**). Persist MAY be ON only when **Maximum history size** is ≤ **4096 KiB**. When size is set above 4096 KiB, the host SHALL automatically set persist to **false** and save that with hardware settings.

#### Scenario: Default persist on S3
- **WHEN** an S3 host loads hardware settings with no prior persist flag stored and Maximum history size is ≤ 4096 KiB
- **THEN** persist historical on internal flash is true

#### Scenario: Default false when size already over gate
- **WHEN** an S3 host loads hardware settings with no prior persist flag and Maximum history size is greater than 4096 KiB
- **THEN** persist historical on internal flash is false

#### Scenario: Size gate blocks ON
- **WHEN** Maximum history size is 8192 KiB and the operator attempts to enable persist on internal flash
- **THEN** the host rejects or forces persist off so persist remains false

#### Scenario: Raising size auto-turns persist OFF
- **WHEN** persist is ON and Maximum history size is changed to a value greater than 4096 KiB
- **THEN** the host sets persist to false and saves hardware settings accordingly

### Requirement: S3 flash persist mode behavior
When S3 persist historical on internal flash is ON, the host SHALL use LittleFS as the durable history snapshot store with periodic saves and keep-after-restore. When persist is OFF, the host SHALL keep the existing reboot-path LittleFS flush and delete-after-restore behavior (no durable periodic flash persist).

#### Scenario: Persist ON uses durable LittleFS path
- **WHEN** persist is ON on an S3 host
- **THEN** history uses LittleFS with periodic interval saves and does not delete the snapshot after a successful boot restore

#### Scenario: Persist OFF keeps one-shot LittleFS path
- **WHEN** persist is OFF on an S3 host
- **THEN** history keeps reboot-path LittleFS flush and deletes the snapshot after a successful boot restore

### Requirement: S3 flash persist periodic save interval
When S3 persist historical on internal flash is ON, the host SHALL write a full history snapshot to LittleFS on the configured Hardware interval (minutes). The payload SHALL match the pre-reboot snapshot format. The interval SHALL be at least **30** and at most **600** minutes; factory default SHALL be **120**. Changing the interval SHALL apply without requiring a reboot for the next countdown cycle. When persist is OFF, this periodic LittleFS interval save SHALL NOT run.

#### Scenario: Default interval
- **WHEN** persist is ON and the flash backup interval was never saved
- **THEN** the effective interval is 120 minutes

#### Scenario: Interval elapsed
- **WHEN** persist is ON, the interval is 120, and 120 minutes elapse since the last successful LittleFS snapshot (or since boot if none yet)
- **THEN** the host writes a full history snapshot to LittleFS

#### Scenario: Persist off skips periodic save
- **WHEN** persist is OFF
- **THEN** the host does not run the flash persist interval save cycle

### Requirement: S3 flash persist keeps snapshot after boot restore
When S3 persist historical on internal flash is ON and boot restores a valid LittleFS history snapshot into PSRAM, the host SHALL leave that snapshot file on internal flash (SHALL NOT delete it after successful restore). When persist is OFF, after a successful LittleFS restore the host SHALL continue to delete the LittleFS snapshot as today. Corrupt LittleFS snapshots MAY still be discarded so a bad file does not loop forever.

#### Scenario: Persist ON keeps files
- **WHEN** persist is ON and boot successfully restores history from the LittleFS snapshot
- **THEN** the LittleFS snapshot file(s) remain on internal flash

#### Scenario: Persist OFF deletes after restore
- **WHEN** persist is OFF and boot successfully restores a LittleFS snapshot into PSRAM
- **THEN** the LittleFS snapshot file(s) are removed before normal operation continues

### Requirement: Status time before save for S3 flash persist
When S3 persist historical on internal flash is ON, the gateway status API SHALL expose the remaining time until the next scheduled LittleFS history backup so Status can show **time before save**. When persist is OFF, that flash-persist countdown field SHALL be omitted or null and Status SHALL NOT show a misleading countdown for flash persist.

#### Scenario: Countdown when persist ON
- **WHEN** persist is ON and 15 minutes remain before the next flash backup
- **THEN** status exposes a remaining-time value operators can show as time before save

### Requirement: Status exposes real on-flash history size
When S3 persist historical on internal flash is ON, the gateway status API SHALL expose the **real** LittleFS history snapshot size in KiB (file size on flash, not PSRAM slab). The host SHALL cache that size during existing restore/save sessions. Status MUST NOT open LittleFS solely to measure size. Missing snapshot → zero or omitted; persist OFF → omitted or null.

#### Scenario: Cached size after save
- **WHEN** persist is ON and a successful save wrote a 200 KiB LittleFS snapshot
- **THEN** Historical storage can show On-flash history size 200 kB from cache without another filesystem open solely for size

#### Scenario: Cached size after boot restore
- **WHEN** persist is ON and boot restore reads a snapshot file of known length
- **THEN** the cached on-flash history size reflects that file’s real size for later Status polls

## MODIFIED Requirements

### Requirement: Delete snapshot after successful restore
After the host successfully restores a **LittleFS** snapshot into PSRAM (including after any required shrink), it SHALL delete the LittleFS snapshot so a later boot does not reload stale data — **except** when S3 **persist historical on internal flash** is ON, in which case the host SHALL leave the LittleFS snapshot in place after successful restore. If restore fails, the host MAY delete a corrupt LittleFS snapshot; it MUST NOT leave a half-applied PSRAM store paired with an unread LittleFS snapshot that would double-apply on the next boot. When the **SD** history persist path is active, the host SHALL restore from SD without deleting the SD snapshot after success (see SD restore requirement).

#### Scenario: Snapshot removed after load
- **WHEN** boot restores a valid **LittleFS** snapshot into PSRAM and S3 persist historical on internal flash is OFF (or the host is not using S3 flash persist)
- **THEN** the LittleFS snapshot file(s) are removed before normal operation continues

#### Scenario: Flash persist keeps LittleFS snapshot after load
- **WHEN** boot restores a valid **LittleFS** snapshot into PSRAM and S3 persist historical on internal flash is ON
- **THEN** the LittleFS snapshot file(s) are not removed after restore

#### Scenario: SD snapshot kept after load
- **WHEN** SD persist is active and boot restores a valid SD snapshot into PSRAM
- **THEN** the SD snapshot file is not removed solely because restore succeeded
