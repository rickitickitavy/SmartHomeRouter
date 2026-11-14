# Spec Delta

## ADDED Requirements

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

## MODIFIED Requirements

### Requirement: Delete snapshot after successful restore
After the host successfully restores a **LittleFS** snapshot into PSRAM (including after any required shrink), it SHALL delete the LittleFS snapshot so a later boot does not reload stale data. If restore fails, the host MAY delete a corrupt LittleFS snapshot; it MUST NOT leave a half-applied PSRAM store paired with an unread LittleFS snapshot that would double-apply on the next boot. When the **SD** history persist path is active, the host SHALL restore from SD without deleting the SD snapshot after success (see SD restore requirement).

#### Scenario: Snapshot removed after load
- **WHEN** boot restores a valid **LittleFS** snapshot into PSRAM
- **THEN** the LittleFS snapshot file(s) are removed before normal operation continues

#### Scenario: SD snapshot kept after load
- **WHEN** SD persist is active and boot restores a valid SD snapshot into PSRAM
- **THEN** the SD snapshot file is not removed solely because restore succeeded

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
