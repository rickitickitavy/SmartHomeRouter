## ADDED Requirements

### Requirement: Settings restore clamps bounded numerics to firmware min/max
On settings restore, bounded numeric Hardware fields in the file SHALL clamp to this firmware’s declared min/max (`>` max → max; `<` min → min). Restore SHALL NOT fail solely for out-of-range. Invalid history Entry size SHALL map to the nearest allowed choice (tie → default). Not a PSRAM-safe slab cap.

#### Scenario: History size above max clamps on restore
- **WHEN** a restore file’s hardware has `historyMaxSizeKiB` greater than this firmware’s `HISTORY_MAX_SIZE_KIB_MAX` and the operator confirms restore
- **THEN** the host stores `HISTORY_MAX_SIZE_KIB_MAX` and does not fail the hardware apply for that reason alone

#### Scenario: History days below min clamps on restore
- **WHEN** a restore file’s hardware has `historyMaxStoreDays` less than 1 and the operator confirms restore
- **THEN** the host stores 1 (the firmware minimum)

#### Scenario: Invalid entry size snaps to allowed choice
- **WHEN** a restore file’s hardware has `historyEntrySize` 300 and the operator confirms restore
- **THEN** the host stores the nearest allowed entry size among 128/256/384/512/768/1024/1536/2048 (or factory default on a tie)

## MODIFIED Requirements

### Requirement: Maintenance restores settings after confirm
Restore settings SHALL ask the operator to confirm before applying a file. After confirm, the host SHALL replace MQTT, Zigbee, hardware SPI speed, blue/green LED brightness, **history retention/slab fields**, and on P4 the **SD history backup interval** when present in the file, and the registered device list from the file. When `ui.theme` is present, the host SHALL apply it to the **signed-in user** only. When a users list is present, the host SHALL replace the users table from that list (hashes and salts, never plaintext passwords). When a consoles list is present, the host SHALL replace control consoles from that list. When an automations list is present, the host SHALL replace automations from that list. When a cameras list is present, the host SHALL replace the camera registry from that list (including ONVIF username and password). When a scenes list is present, the host SHALL replace the scene registry from that list (model and pins; bake JPEGs kept only when id and bakePath still match). The host SHALL leave Wi-Fi unchanged even if the file contains a wifi object. A file that is not valid JSON, or that lacks a usable settings body, SHALL be rejected and SHALL NOT write settings. Only `isAdmin` SHALL restore settings. Devices Export/Restore on the Devices page SHALL remain. A restore file whose `hardware` object lacks brightness fields SHALL keep the host’s current brightness values. A restore file whose `hardware` object lacks the SD backup interval SHALL keep the host’s current interval (or factory default if never set). Bounded numeric Hardware fields present in the file SHALL be clamped to this firmware’s min/max (or discrete allowed set) rather than rejecting restore for being out of range.

#### Scenario: Confirm then apply
- **WHEN** the operator chooses Restore settings, selects a valid export file, and confirms
- **THEN** MQTT, Zigbee, hardware, theme for the signed-in user, devices, and users (when present in the file) match the file and Wi-Fi settings are unchanged

#### Scenario: Restore applies LED brightness
- **WHEN** the file’s hardware has `blueLedBrightness` 25 and `greenLedBrightness` 80 and the operator confirms
- **THEN** those brightness values are stored and applied on the host (and pushed to the slave when linked)

#### Scenario: Restore applies SD backup interval
- **WHEN** a P4 restore file’s hardware has SD history backup interval 45 and the operator confirms
- **THEN** that interval is stored and used for the next SD backup countdown

#### Scenario: Cancel confirm
- **WHEN** the restore confirm dialog is visible and the operator cancels
- **THEN** no settings or devices are written

#### Scenario: Wi-Fi in file is ignored
- **WHEN** the file includes a wifi password different from the running host and the operator confirms restore
- **THEN** the host Wi-Fi password is unchanged

#### Scenario: Users in file replace the table
- **WHEN** an `isAdmin` operator confirms restore of a valid file whose users list differs from the host
- **THEN** the host users table matches the file (hashes, not plaintext passwords)

#### Scenario: Consoles in file replace layouts
- **WHEN** an `isAdmin` operator confirms restore of a valid file that includes consoles
- **THEN** control consoles on the host match that file

#### Scenario: Automations in file replace definitions
- **WHEN** an `isAdmin` operator confirms restore of a valid file that includes automations
- **THEN** host automations match that file’s persisted definitions

#### Scenario: Cameras in file replace registry with credentials
- **WHEN** an `isAdmin` operator confirms restore of a valid file that includes cameras with usernames and passwords
- **THEN** the host camera registry matches that file, including each camera’s username and password

#### Scenario: Out-of-range history size clamps on restore
- **WHEN** the file’s hardware has `historyMaxSizeKiB` above this firmware’s maximum and the operator confirms
- **THEN** restore succeeds with that field stored at the firmware maximum

#### Scenario: Out-of-range brightness does not reject restore
- **WHEN** the file’s hardware has `blueLedBrightness` 0 or 101 and the operator confirms
- **THEN** restore succeeds and that brightness is stored as the factory default (100)

### Requirement: Hardware tab sets blue and green LED brightness
System → Hardware SHALL show SPI speed plus **BLUE LED brightness** and **GREEN LED brightness** as integer fields from 1 to 100 (percent), a checkbox **use queue for base device command** (square-only toggle; default unchecked / false), plus the history fields **Max History store days**, **History sweep interval (minutes)**, **Maximum history size** (kB), and **Entry size** (hard choice 128/256/384/512/768/1024/1536/2048). Each Entry size choice SHALL show how many pages that size can address and how many number (4-byte), logic (1-byte), and string (31-character) values one series can store. GET `/api/hardware` SHALL return `spiSpeedHz`, `blueLedBrightness`, `greenLedBrightness`, `useQueueForBaseDeviceCommand` (boolean), and the history fields. POST `/api/hardware` SHALL require the brightness and SPI fields as today, accept the boolean (omitted on older clients MUST mean false), accept the history fields (omitted on older clients MUST keep prior stored values or factory defaults), reject values outside 1–100 for either brightness, **clamp** out-of-range history days, sweep interval, maximum history size, and backup intervals to this firmware’s min/max (and map invalid Entry size to the nearest allowed choice), persist them with hardware settings, apply brightness without requiring a reboot, push brightness to the slave when the link is up, and persist the queue flag for use by the action/command FIFO. Factory default for both brightness fields SHALL be 100. Factory default for **use queue for base device command** SHALL be false. Factory defaults for history SHALL be days 7, sweep 10 minutes, Entry size 1024, and Maximum history size **3072** KiB. Only `isAdmin` SHALL use Hardware.

#### Scenario: Load shows brightness
- **WHEN** an admin opens System → Hardware
- **THEN** blue and green brightness show the stored values (default 100 if never saved) and the queue checkbox reflects the stored flag (default unchecked)

#### Scenario: Save applies brightness
- **WHEN** the admin sets blue to 40 and green to 70 and clicks Save
- **THEN** GET `/api/hardware` returns those values and host blue/green LEDs use them while on

#### Scenario: Out of range rejected
- **WHEN** POST `/api/hardware` sends `blueLedBrightness` 0 or 101
- **THEN** the host responds 400 and does not change stored brightness

#### Scenario: Save queue flag
- **WHEN** the admin checks **use queue for base device command** and saves
- **THEN** GET `/api/hardware` returns `useQueueForBaseDeviceCommand` true and base UI device commands use the action/command FIFO

#### Scenario: History fields round-trip
- **WHEN** the admin sets Max History store days to 10, sweep interval to 15, Maximum history size to 512, Entry size to 512, and saves
- **THEN** GET `/api/hardware` returns those history values

#### Scenario: History size above max clamps on Hardware save
- **WHEN** POST `/api/hardware` sends `historyMaxSizeKiB` greater than this firmware’s maximum
- **THEN** the host stores the firmware maximum and responds success (does not 400 solely for that field)
