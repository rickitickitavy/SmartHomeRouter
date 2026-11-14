# Spec Delta

## ADDED Requirements

### Requirement: Hardware sets SD history backup interval on P4
On the P4 host console, System → Hardware SHALL show **interval for backup historical data to SD** as an integer minutes field from **5** to **600** (factory default **60**). GET `/api/hardware` SHALL return `historySdBackupIntervalMin` (or an equivalent documented key). POST `/api/hardware` SHALL accept it, reject out-of-range values, and persist it with hardware settings. On non-P4 hosts the field SHALL be omitted from Hardware UI and NEED NOT appear in the API. Omitted on older clients MUST keep the prior stored value or factory default. Only `isAdmin` SHALL use Hardware.

#### Scenario: Default interval
- **WHEN** a P4 admin opens Hardware and the SD backup interval was never saved
- **THEN** the field shows 60

#### Scenario: Save interval
- **WHEN** the admin sets the SD backup interval to 120 and saves
- **THEN** GET `/api/hardware` returns 120

#### Scenario: Reject out of range
- **WHEN** POST `/api/hardware` sends SD backup interval 4 or 601
- **THEN** the host responds 400 and does not change the stored interval

### Requirement: Status Devices group follows Free memory
On Status (operator and simple-user), the **Devices** group SHALL appear immediately after the **Free memory** group for all users.

#### Scenario: Devices after Free memory
- **WHEN** Status is open for an admin or a simple user
- **THEN** Devices is listed directly below Free memory

### Requirement: Status SD card group
On the P4 host, Status SHALL show an **SD card** group with **status**, **capacity Mb**, and **free Mb** from the gateway status API. Operator Status and simple-user Status MUST both show this group when the viewer can open Status on P4. Non-P4 builds SHALL omit the group.

#### Scenario: Available card figures
- **WHEN** Status is open on P4 and a usable SD card was found (even if currently unmounted between saves) with known capacity and free space
- **THEN** SD card shows status plus capacity Mb and free Mb

#### Scenario: Absent card
- **WHEN** Status is open on P4 and no usable SD was found
- **THEN** SD card shows an absent/error status and does not claim capacity as if a card were present

## MODIFIED Requirements

### Requirement: Status Historical storage group
Status SHALL show a **Historical storage** group with entries count, pages count, PSRAM usage in kibibytes (1024-byte KiB), and values stored (total samples across all pages). When the SD history persist path is active, the group SHALL also show **time before save** (remaining time until the next scheduled SD history backup) and the **real on-SD history size** in kibibytes when that cached size is available (file size on the card, not PSRAM slab). When SD persist is not active, **time before save** and on-SD history size SHALL NOT be shown. Values SHALL come from the gateway status API (cached; no SD remount for Status). Status SHALL NOT offer history settings forms. Operator Status and simple-user Status MUST both show this group when the viewer can open Status.

#### Scenario: Counts visible
- **WHEN** Status is open and the host has 2 history entries, 5 pages, 20 KiB history slab usage, and 100 stored values
- **THEN** Historical storage shows those four figures

#### Scenario: Countdown when SD persist active
- **WHEN** Status is open, SD history persist is active, and 12 minutes remain before the next SD backup
- **THEN** Historical storage shows time before save reflecting that remaining time

#### Scenario: On-SD size when SD persist active
- **WHEN** Status is open, SD history persist is active, and the cached real snapshot size on SD is 150 KiB
- **THEN** Historical storage shows that on-SD size in kB on operator and simple-user Status

#### Scenario: No countdown without SD persist
- **WHEN** Status is open and SD history persist is not active
- **THEN** Historical storage does not show time before save or on-SD history size

### Requirement: Maintenance exports settings without Wi-Fi
System → Maintenance SHALL show **Export settings** above **Restore settings**. Both SHALL use compact inline buttons, not full-width bars. Export SHALL download a JSON file that includes MQTT settings, Zigbee settings, hardware SPI speed and blue/green LED brightness, **history retention/slab fields**, and on P4 the **SD history backup interval** when that setting exists, the signed-in user’s theme, the persisted device list (same device fields as Devices Export, no live telemetry), the users table, control consoles, automations (persisted fields only, no last-activation or TIMER runtime telemetry), cameras (same ONVIF fields as Cameras Export, including each camera’s username and password), and scenes (same fields as Scenes store export: id, name, sceneModel, pins, bakePath/hasBake — bake JPEG bytes are not embedded). Each exported user SHALL include user name, added-at, roles including `editConsoles`, `consoles`, `isBlocked`, theme, and password **hash and salt**. The file SHALL NOT contain a plaintext user password. The file SHALL NOT contain a Wi-Fi group (no BSSID, password, MODE, AP IP, hostname, or OTG). Only `isAdmin` SHALL see or use Export settings and Restore settings.

#### Scenario: Export omits Wi-Fi
- **WHEN** the operator clicks Export settings and the host has Wi-Fi, MQTT, Zigbee, hardware, a saved theme, and at least one registered device
- **THEN** the downloaded JSON has mqtt, zigbee, hardware, ui theme, and devices, and has no wifi object or Wi-Fi password

#### Scenario: Export includes LED brightness
- **WHEN** the operator exports settings after saving blue and green brightness
- **THEN** `hardware` in the JSON includes `blueLedBrightness` and `greenLedBrightness`

#### Scenario: Export includes SD backup interval on P4
- **WHEN** a P4 admin has saved SD history backup interval 90 and exports settings
- **THEN** `hardware` in the JSON includes that interval

#### Scenario: Buttons stay compact
- **WHEN** the operator opens System → Maintenance
- **THEN** Export settings and Restore settings are stacked and no wider than a normal inline button

#### Scenario: Export includes users without plaintext passwords
- **WHEN** an `isAdmin` operator clicks Export settings and the host has at least one stored user
- **THEN** the downloaded JSON includes those users with hashes (and salts) and does not include any plaintext user password

#### Scenario: Export includes consoles
- **WHEN** the host has a saved control console and an `isAdmin` operator exports settings
- **THEN** the JSON includes that console’s name, active flag, and layout

#### Scenario: Export includes automations
- **WHEN** the host has a saved automation and an `isAdmin` operator exports settings
- **THEN** the JSON includes that automation’s persisted fields and omits last-activation and TIMER runtime telemetry

#### Scenario: Export includes cameras with credentials
- **WHEN** the host has a saved ONVIF camera with username and password and an `isAdmin` operator exports settings
- **THEN** the JSON `cameras` array includes that camera with the same username and password

#### Scenario: Export includes scenes
- **WHEN** the host has a saved scene and an `isAdmin` operator exports settings
- **THEN** the JSON `scenes` array includes that scene’s id, name, sceneModel, and pins

### Requirement: Maintenance restores settings after confirm
Restore settings SHALL ask the operator to confirm before applying a file. After confirm, the host SHALL replace MQTT, Zigbee, hardware SPI speed, blue/green LED brightness, **history retention/slab fields**, and on P4 the **SD history backup interval** when present in the file, and the registered device list from the file. When `ui.theme` is present, the host SHALL apply it to the **signed-in user** only. When a users list is present, the host SHALL replace the users table from that list (hashes and salts, never plaintext passwords). When a consoles list is present, the host SHALL replace control consoles from that list. When an automations list is present, the host SHALL replace automations from that list. When a cameras list is present, the host SHALL replace the camera registry from that list (including ONVIF username and password). When a scenes list is present, the host SHALL replace the scene registry from that list (model and pins; bake JPEGs kept only when id and bakePath still match). The host SHALL leave Wi-Fi unchanged even if the file contains a wifi object. A file that is not valid JSON, or that lacks a usable settings body, SHALL be rejected and SHALL NOT write settings. Only `isAdmin` SHALL restore settings. Devices Export/Restore on the Devices page SHALL remain. A restore file whose `hardware` object lacks brightness fields SHALL keep the host’s current brightness values. A restore file whose `hardware` object lacks the SD backup interval SHALL keep the host’s current interval (or factory default if never set).

#### Scenario: Confirm then apply
- **WHEN** the operator chooses Restore settings, selects a valid export file, and confirms
- **THEN** MQTT, Zigbee, hardware, theme for the signed-in user, devices, and users (when present in the file) match the file and Wi-Fi settings are unchanged

#### Scenario: Restore applies LED brightness
- **WHEN** the file’s hardware has `blueLedBrightness` 25 and `greenLedBrightness` 80 and the operator confirms
- **THEN** those brightness values are stored and applied on the host (and pushed to the slave when linked)

#### Scenario: Restore applies SD backup interval
- **WHEN** a P4 restore file’s hardware has SD history backup interval 45 and the operator confirms
- **THEN** that interval is stored and used for the next SD backup countdown
