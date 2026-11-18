# Spec Delta: web-console

## ADDED Requirements

### Requirement: Status General labels RF firmware
Status **General** SHALL label the slave firmware version row **RF firmware** (not **Slave**) on both the operator page and the simple-user page.

#### Scenario: Operator General uses RF firmware
- **WHEN** an admin opens Status on the operator console
- **THEN** General shows Online time, Master, and RF firmware

#### Scenario: Simple-user General matches
- **WHEN** a simple user opens Status
- **THEN** General uses the same **RF firmware** label as the operator page

### Requirement: Status pages match for operator and simple user
When both the operator console and the simple-user page can show Status, the Status group titles, row labels, and row visibility rules for General, Interconnection, Free memory, CPU, Internal database (and related storage groups), Devices, MQTT, Network, Connections, and WiFi SHALL be identical. Layout MAY differ (two-column vs single-column) without changing those labels or which rows appear for the same payload.

#### Scenario: Matching history and firmware labels
- **WHEN** Status is open on both pages for the same host payload
- **THEN** both show Internal database (not Historical storage), rows stored (not Values stored), on-SD size (not on-SD history size), and RF firmware (not Slave)

### Requirement: Status Interconnection widget shows queue and Repeater versions
Status **Interconnection** SHALL show interconnect queue used and depth as **Queue: used / depth** (or equivalent single row). On Main with paired Repeaters, each Repeater row SHALL include that peer’s reported host VERSION when known. The Interconnection settings tab SHALL NOT show the queue used/depth line as its primary placement for that value.

#### Scenario: Queue on Status not settings tab
- **WHEN** an admin opens Status and System → Interconnection on Main
- **THEN** Status Interconnection shows Queue used/depth and the Interconnection tab does not present that Queue line as the operator-facing queue status

#### Scenario: Repeater version on Status
- **WHEN** Main has a paired Repeater that reported host version `S3-4.4.20`
- **THEN** Status Interconnection shows that version with that Repeater

### Requirement: Paired Repeater dialog omits Devices and Online
The paired-Repeater view dialog on Main SHALL NOT show **Devices** or **Online** fields. Name, MAC, IP, Port, Version, router id, and Zigbee channel (and action controls) MAY remain.

#### Scenario: Dialog without Devices Online
- **WHEN** an admin opens a paired Repeater dialog on Main
- **THEN** Devices and Online rows are absent from that dialog

### Requirement: Search devices dialog width fits headers
The Search devices dialog SHALL be wide enough that the found-table column headers (including **Router**) and typical cell values (IEEE, Type, Manufacturer, Model) fit on one line without wrapping under normal desktop viewport widths used for the operator console.

#### Scenario: Router header one line
- **WHEN** Search devices is open with at least one found row on a normal desktop width
- **THEN** the Router column header and the IEEE and Type cells for that row remain on a single line each

### Requirement: System Internal DB tab after Hardware
When System is selected for an admin, the console MUST present nested folder-style tabs that include **Internal DB** immediately after **Hardware**. Selecting Internal DB MUST show historical-data storage settings and hide the other System panels without a full page reload. Only `isAdmin` MUST use Internal DB.

#### Scenario: Tab order places Internal DB after Hardware
- **WHEN** an admin opens System and inspects nested tab order
- **THEN** Internal DB appears immediately after Hardware

#### Scenario: Internal DB opens history storage settings
- **WHEN** an admin selects System → Internal DB
- **THEN** historical storage settings (retention, slab size, entry size, flash/SD persist controls as applicable) are shown in that panel

## MODIFIED Requirements

### Requirement: System has Update and Log tabs
When System is selected, the console MUST present nested folder-style tabs in this order: Log, Interconnection, devices.json, Update, Hardware, Internal DB. The default System panel MUST be Log. Selecting a tab MUST show that panel and hide the others without a full page reload.

#### Scenario: Nested System tabs
- **WHEN** the operator opens System and then selects Log
- **THEN** the Log panel is shown and the Update panel is hidden

#### Scenario: Default System tab is Log
- **WHEN** the operator opens System
- **THEN** the Log tab is selected first

#### Scenario: Interconnection follows Log
- **WHEN** the operator opens System and inspects nested tab order
- **THEN** Interconnection appears immediately after Log

#### Scenario: Internal DB follows Hardware
- **WHEN** the operator opens System and inspects nested tab order
- **THEN** Internal DB appears immediately after Hardware

### Requirement: Status groups use compact name-value rows
Status SHALL render live attributes as groups on a two-column Status stack (one column when the viewport is narrow). Column 1 SHALL place **General**, then **Free memory**, then **CPU** (host busy percents), then **Internal database** (and related storage groups). Column 2 SHALL place **Devices** at the top, then **MQTT**, then **Network**, then **Connections**, then remaining groups such as **WiFi**. Each group SHALL have a heading. Each attribute SHALL keep its name and value on a single row. Values in a group SHALL align to the right. Groups SHALL use a mindful width (content plus padding) and SHALL NOT stretch to the full main-panel width. Status SHALL NOT contain settings forms that write configuration.

#### Scenario: Grouped Status layout
- **WHEN** the operator opens Status on a wide viewport
- **THEN** column 1 shows Free memory then CPU (among its groups), and column 2 starts with Devices, each group narrower than the main panel, with names on the left and values on the right of the same row

### Requirement: Status Historical storage group
Status SHALL show an **Internal database** group with entries count, pages count, PSRAM usage in kibibytes (1024-byte KiB), and **rows stored** (total samples across all pages). When the SD history persist path is active, the group SHALL also show **time before save** (remaining time until the next scheduled SD history backup) and the **real on-SD size** in kibibytes when that cached size is available (file size on the card, not PSRAM slab). When SD persist is not active and S3 flash persist is not ON, **time before save** and on-SD size SHALL NOT be shown. Values SHALL come from the gateway status API (cached; no SD remount for Status). Status SHALL NOT offer history settings forms. Operator Status and simple-user Status MUST both show this group when the viewer can open Status.

#### Scenario: Counts visible
- **WHEN** Status is open and the host has 2 history entries, 5 pages, 20 KiB history slab usage, and 100 stored values
- **THEN** Internal database shows those four figures with the rows-stored label

#### Scenario: Countdown when SD persist active
- **WHEN** Status is open, SD history persist is active, and 12 minutes remain before the next SD backup
- **THEN** Internal database shows time before save reflecting that remaining time

#### Scenario: On-SD size when SD persist active
- **WHEN** Status is open, SD history persist is active, and the cached real snapshot size on SD is 150 KiB
- **THEN** Internal database shows that on-SD size in kB on operator and simple-user Status

#### Scenario: No countdown without SD persist
- **WHEN** Status is open and SD history persist is not active and S3 flash persist is not ON
- **THEN** Internal database does not show time before save or on-SD size

### Requirement: Hardware sets SD history backup interval on P4
On the P4 host console, System → Internal DB SHALL show **interval for backup historical data to SD** as an integer minutes field from **5** to **600** (factory default **60**). GET `/api/hardware` SHALL return `historySdBackupIntervalMin` (or an equivalent documented key). POST `/api/hardware` (or the Internal DB save path that persists the same keys) SHALL accept it, reject out-of-range values, and persist it with hardware/history settings. On non-P4 hosts the field SHALL be omitted from Internal DB UI and NEED NOT appear in the API. Omitted on older clients MUST keep the prior stored value or factory default. Only `isAdmin` SHALL use Internal DB for these settings. Hardware SHALL NOT host this field after the move.

#### Scenario: Default interval
- **WHEN** a P4 admin opens Internal DB and the SD backup interval was never saved
- **THEN** the field shows 60

#### Scenario: Save interval
- **WHEN** the admin sets the SD backup interval to 120 and saves
- **THEN** GET `/api/hardware` returns 120

#### Scenario: Reject out of range
- **WHEN** POST `/api/hardware` sends SD backup interval 4 or 601
- **THEN** the host responds 400 and does not change the stored interval

### Requirement: Hardware sets S3 persist historical on internal flash
On the ESP32-S3 host console, System → Internal DB SHALL show a checkbox **persist historical on internal flash** (square-only; default true when no SD card is present). Persist MAY be ON only when Maximum history size is ≤ 4096 KiB and no usable SD card is present. When an SD card is present at boot (or when SD becomes the active history path), the host SHALL set persist OFF, persist that, and the UI SHALL hide the checkbox. `/api/hardware` SHALL round-trip `historyPersistFlash`. Include in global settings export/restore. Omit on non-S3 hosts. Visible on Main and on Repeater when applicable (not hidden solely because role is Repeater). Only `isAdmin`.

#### Scenario: Default checkbox on S3
- **WHEN** an S3 admin opens Internal DB, no SD card is present, persist was never saved, and Maximum history size is ≤ 4096
- **THEN** persist historical on internal flash is checked and visible

#### Scenario: Size above gate disables ON
- **WHEN** Maximum history size is 8192 and the admin tries to check persist historical on internal flash
- **THEN** the UI/API does not leave persist ON

#### Scenario: Save with large size clears persist
- **WHEN** persist is ON and the admin saves Maximum history size greater than 4096
- **THEN** after save, persist is OFF

#### Scenario: SD present hides and disables persist
- **WHEN** an S3 host boots with a usable SD card present
- **THEN** persist historical on internal flash is OFF and the Internal DB checkbox is hidden

#### Scenario: Repeater still shows persist when no SD
- **WHEN** network role is Repeater on S3, no SD is present, and size allows persist
- **THEN** Internal DB shows persist historical on internal flash

### Requirement: Hardware sets S3 flash history save interval
On the ESP32-S3 host console, when persist historical on internal flash is ON, System → Internal DB SHALL show **interval save historical data** (30–600 minutes, default 120). `/api/hardware` SHALL round-trip `historyFlashBackupIntervalMin` and reject out-of-range values. Hide or disable when persist is OFF. Omit on non-S3 hosts. Include in global settings export/restore. Only `isAdmin`. Hardware SHALL NOT host this field after the move.

#### Scenario: Default interval when persist ON
- **WHEN** an S3 admin opens Internal DB with persist ON and the flash interval was never saved
- **THEN** the interval shows 120

#### Scenario: Persist OFF hides interval
- **WHEN** persist historical on internal flash is OFF
- **THEN** the interval save historical data field is not shown as an editable flash-persist control

#### Scenario: Out of range rejected
- **WHEN** the admin POSTs a flash backup interval of 10 minutes
- **THEN** the API rejects the value (below 30)

### Requirement: Status shows On-flash history size in Historical storage
When S3 persist historical on internal flash is ON, Status **Internal database** SHALL show **On-flash history size** in kibibytes from the status API cached real LittleFS snapshot size (same role as P4 **on-SD size**). When persist is OFF, that row SHALL NOT be shown. Operator Status and simple-user Status MUST both show this when the viewer can open Status and persist is ON.

#### Scenario: Persist ON shows On-flash size
- **WHEN** an operator opens Status, S3 flash persist is ON, and cached on-flash size is 200 KiB
- **THEN** Internal database shows On-flash history size as 200 kB (or equivalent formatting)

#### Scenario: Persist OFF hides On-flash size
- **WHEN** S3 flash persist is OFF
- **THEN** Internal database does not show On-flash history size

### Requirement: Status File system shows Historical data size on S3
On S3 hosts only, when persist historical on internal flash is ON, Status **File system** SHALL show **Historical data size** using the same cached on-flash history size as **On-flash history size** in Internal database. When persist is OFF, that File system row SHALL NOT be shown. Non-S3 hosts SHALL NOT show this File system row. Operator and simple-user Status MUST match.

#### Scenario: S3 persist ON duplicates size under File system
- **WHEN** an S3 operator opens Status with flash persist ON and cached size 200 KiB
- **THEN** File system shows Historical data size matching On-flash history size

#### Scenario: Non-S3 omits File system historical size
- **WHEN** a P4 operator opens Status
- **THEN** File system does not show Historical data size (P4 keeps on-SD size only under Internal database)

### Requirement: Hardware history retention and slab size
System → Internal DB SHALL show Max History store days (1–20, default 7), History sweep interval minutes (1–120, default 10), Maximum history size in KiB (default **3072**), and **Entry size** as a hard choice of 128, 256, 384, 512, 768, 1024, 1536, or 2048 (default 1024). `/api/hardware` SHALL round-trip these fields, clamp out-of-range days, sweep interval, and maximum size to the firmware min/max, map an invalid Entry size to the nearest allowed choice (tie → 1024), and apply slab size and entry size at next boot. Only `isAdmin` SHALL use Internal DB for these settings. Hardware SHALL NOT host these history retention/slab fields after the move.

#### Scenario: Defaults on first open
- **WHEN** an admin opens Internal DB and history settings were never saved
- **THEN** days show 7, sweep interval shows 10, Maximum history size shows 3072, and Entry size shows 1024

#### Scenario: Out-of-range days clamp
- **WHEN** POST `/api/hardware` sends Max History store days `0` or `21`
- **THEN** the host stores the firmware minimum or maximum and does not respond 400 for that field

#### Scenario: Invalid entry size snaps
- **WHEN** POST `/api/hardware` sends Entry size `300`
- **THEN** the host stores the nearest allowed size and does not respond 400 for that field

### Requirement: Repeater role hides blocked console sections
When network role is Repeater, the console MUST hide sidebar sections Place, Devices, Automation, Cameras, Scenes, Consoles, and Security. System → Maintenance MUST hide all Maintenance cards except **Reboot**. System → Hardware MUST show only SPI SPEED, RF update SPI speed, BLUE LED brightness, GREEN LED brightness, use queue for base device commands, and Low memory threshold (and their Save/Reset as applicable). Other Hardware fields MUST be hidden on Repeater. System → Internal DB MUST remain available on Repeater for historical storage settings that apply to that role (including persist historical on internal flash when shown).

#### Scenario: Repeater hides Devices
- **WHEN** network role is Repeater and an admin is signed in
- **THEN** Devices is not shown in the sidebar

#### Scenario: Repeater keeps Reboot
- **WHEN** network role is Repeater and an admin opens System → Maintenance
- **THEN** Reboot remains available and export/restore settings are hidden

#### Scenario: Repeater Hardware allow-list
- **WHEN** network role is Repeater and an admin opens System → Hardware
- **THEN** SPI speed, RF update SPI speed, blue/green LED brightness, use queue for base device commands, and low memory threshold are visible and other Hardware groups are hidden

#### Scenario: Repeater keeps Internal DB
- **WHEN** network role is Repeater and an admin opens System
- **THEN** Internal DB is available for historical storage settings

### Requirement: Maintenance Interconnection export and import
System → Maintenance SHALL show an **Interconnection** card with **Export** and **Import** for `isAdmin` on Main. Export SHALL download interconnect JSON matching the global interconnect slice including paired Repeaters. Import SHALL confirm then **fully override** the paired-router table from the file (existing peers are not MAC-authoritative keepers). Hide the card on Repeater.

#### Scenario: Export interconnect file
- **WHEN** an admin exports Interconnection from Maintenance on a Main with one paired Repeater
- **THEN** the downloaded JSON includes that peer’s persisted interconnect identity fields

#### Scenario: Import keeps matched existing peers
- **WHEN** an admin confirms Import listing a different set of paired MACs than currently stored
- **THEN** the paired table matches the file (within the seven-Repeater cap) and prior peers absent from the file are removed

#### Scenario: Hidden on Repeater
- **WHEN** network role is Repeater and an admin opens Maintenance
- **THEN** the Interconnection export/import card is not shown
