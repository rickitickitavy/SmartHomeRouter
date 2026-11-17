# Spec Delta

## ADDED Requirements

### Requirement: System Interconnection tab after Log
When System is selected, the console MUST show an **Interconnection** nested tab immediately after **Log**. Only `isAdmin` MUST use Interconnection. The tab MUST load and save interconnect settings (router name, network role, pairing port) and MUST show role-specific controls (Main paired table and Pair; Repeater Pair to host and Main status).

#### Scenario: Tab order places Interconnection after Log
- **WHEN** an admin opens System
- **THEN** the nested tabs include Interconnection immediately after Log

#### Scenario: Admin opens Interconnection
- **WHEN** an admin selects System → Interconnection
- **THEN** router name, network role, and pairing port controls are shown

### Requirement: Interconnection Main paired table UI
When network role is Main, Interconnection MUST show the paired-routers table (Name, MAC, IP, Port, online circle, devices, In, Out) and a **Pair** button beneath it that opens the IP/port pairing dialog (Start/Cancel; port default 10241).

#### Scenario: Pair opens dialog
- **WHEN** an admin on Main clicks Pair
- **THEN** a dialog asks for IP (optional) and port with Start and Cancel

### Requirement: Interconnection Repeater pair UI
When network role is Repeater, Interconnection MUST show **Pair to host**, Main online status (green/gray circle), and after pairing the Main IP, name, and MAC. Pair to host MUST show a modal countdown (up to 120 s) with Cancel while waiting.

#### Scenario: Pair to host shows countdown
- **WHEN** an admin on Repeater clicks Pair to host
- **THEN** a modal with countdown and Cancel is visible for the wait

### Requirement: Repeater role hides blocked console sections
When network role is Repeater, the console MUST hide sidebar sections Place, Devices, Automation, Cameras, Scenes, Consoles, and Security. System → Maintenance MUST hide all Maintenance cards except **Reboot**. System → Hardware MUST show only SPI SPEED, RF update SPI speed, BLUE LED brightness, GREEN LED brightness, use queue for base device commands, and Low memory threshold (and their Save/Reset as applicable). Other Hardware fields MUST be hidden on Repeater.

#### Scenario: Repeater hides Devices
- **WHEN** network role is Repeater and an admin is signed in
- **THEN** Devices is not shown in the sidebar

#### Scenario: Repeater keeps Reboot
- **WHEN** network role is Repeater and an admin opens System → Maintenance
- **THEN** Reboot remains available and export/restore settings are hidden

#### Scenario: Repeater Hardware allow-list
- **WHEN** network role is Repeater and an admin opens System → Hardware
- **THEN** SPI speed, RF update SPI speed, blue/green LED brightness, use queue for base device commands, and low memory threshold are visible and other Hardware groups are hidden

### Requirement: Found and device UI show non-local router
When a found or registered Zigbee device has non-zero `router_id`, the Devices / found UI on Main MUST show which paired router owns that device (name or id) so the operator can see remote origin.

#### Scenario: Found row shows repeater
- **WHEN** Main’s found list includes a device with non-zero `router_id`
- **THEN** that row identifies the source router

## MODIFIED Requirements

### Requirement: System has Update and Log tabs

When System is selected, the console MUST present nested folder-style tabs in this order: Log, Interconnection, devices.json, Update, Hardware. The default System panel MUST be Log. Selecting a tab MUST show that panel and hide the others without a full page reload.

#### Scenario: Nested System tabs

- **WHEN** the operator opens System and then selects Log
- **THEN** the Log panel is shown and the Update panel is hidden

#### Scenario: Default System tab is Log
- **WHEN** the operator opens System
- **THEN** the Log tab is selected first

#### Scenario: Interconnection follows Log
- **WHEN** the operator opens System and inspects nested tab order
- **THEN** Interconnection appears immediately after Log

### Requirement: Maintenance exports settings without Wi-Fi
System → Maintenance SHALL show **Export settings** above **Restore settings**. Both SHALL use compact inline buttons, not full-width bars. Export SHALL download a JSON file that includes MQTT settings, Zigbee settings, hardware SPI speed and blue/green LED brightness, **history retention/slab fields**, and on P4 the **SD history backup interval** when that setting exists, the signed-in user’s theme, the persisted device list (same device fields as Devices Export, no live telemetry), the users table, control consoles, automations (persisted fields only, no last-activation or TIMER runtime telemetry), cameras (same ONVIF fields as Cameras Export, including each camera’s username and password), scenes (same fields as Scenes store export: id, name, sceneModel, pins, bakePath/hasBake — bake JPEG bytes are not embedded), and **interconnect** settings (router name, network role, pairing port, checkAlive interval, zigbee search interval owned by this host, and paired-router identity fields needed to restore peering). Each exported user SHALL include user name, added-at, roles including `editConsoles`, `consoles`, `isBlocked`, theme, and password **hash and salt**. The file SHALL NOT contain a plaintext user password. The file SHALL NOT contain a Wi-Fi group (no BSSID, password, MODE, AP IP, hostname, or OTG). Only `isAdmin` SHALL see or use Export settings and Restore settings.

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

#### Scenario: Export includes interconnect
- **WHEN** an `isAdmin` operator exports settings on a host with interconnect role and paired peer data saved
- **THEN** the JSON includes an interconnect slice with those persisted fields

### Requirement: Maintenance restores settings after confirm
Restore settings SHALL ask the operator to confirm before applying a file. After confirm, the host SHALL replace MQTT, Zigbee, hardware SPI speed, blue/green LED brightness, **history retention/slab fields**, and on P4 the **SD history backup interval** when present in the file, and the registered device list from the file. When `ui.theme` is present, the host SHALL apply it to the **signed-in user** only. When a users list is present, the host SHALL replace the users table from that list (hashes and salts, never plaintext passwords). When a consoles list is present, the host SHALL replace control consoles from that list. When an automations list is present, the host SHALL replace automations from that list. When a cameras list is present, the host SHALL replace the camera registry from that list (including ONVIF username and password). When a scenes list is present, the host SHALL replace the scene registry from that list (model and pins; bake JPEGs kept only when id and bakePath still match). When an **interconnect** slice is present, the host SHALL replace interconnect settings and the paired-router table from that slice. The host SHALL leave Wi-Fi unchanged even if the file contains a wifi object. A file that is not valid JSON, or that lacks a usable settings body, SHALL be rejected and SHALL NOT write settings. Only `isAdmin` SHALL restore settings. Devices Export/Restore on the Devices page SHALL remain. A restore file whose `hardware` object lacks brightness fields SHALL keep the host’s current brightness values. A restore file whose `hardware` object lacks the SD backup interval SHALL keep the host’s current interval (or factory default if never set). Bounded numeric Hardware fields present in the file SHALL be clamped to this firmware’s min/max (or discrete allowed set) rather than rejecting restore for being out of range.

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

#### Scenario: Restore applies interconnect
- **WHEN** the file includes an interconnect slice with role Repeater and a paired Main identity and the operator confirms
- **THEN** the host stores that interconnect configuration from the file
