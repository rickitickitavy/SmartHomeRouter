# Spec Delta

## ADDED Requirements

### Requirement: Automation section lists automations
The Automation section MUST show a table of automations (name, active, last performed when known) and buttons Add, Export all, and Import all. Only `isAdmin` MUST see Automation.

#### Scenario: Open Automation list
- **WHEN** an `isAdmin` opens Automation and at least one automation is stored
- **THEN** each row shows name, active, and last performed when known

### Requirement: Automation edit dialog import and export
Add or row edit MUST open an edit dialog with name, title, active, TIMER (`use`, `autoRestart`, `defaultTime`, and runtime `counting` / `timeLeft` when known), listening devices, condition editor, actions (device or automation targets), Export, and Import. Dialog Import MUST be enabled only when the form is empty (no listeners, no actions, empty condition, TIMER `use` false / defaults unset). Dialog Export MUST download one JSON file named from the automation’s `name`.

#### Scenario: Empty-only dialog import
- **WHEN** the edit dialog already has a listener, action, or TIMER `use` true
- **THEN** Import is disabled

#### Scenario: Single export filename
- **WHEN** the operator exports from the edit dialog for automation name `yard-light`
- **THEN** the browser downloads a JSON file named for `yard-light`

### Requirement: Automation actions target devices or automations
The edit dialog Actions editor MUST let each action target a device (payload/channel as Manual command) or an automation. Automation-target ops MUST include ENABLE and DISABLE; when the selected target automation uses a timer, ops MUST also include STOP TIMER, START TIMER, and RESTART TIMER. START TIMER MUST allow an optional countdown time when the target has no default time.

#### Scenario: Automation action ops
- **WHEN** the operator adds an action targeting an automation that uses a timer
- **THEN** the op list includes ENABLE, DISABLE, STOP TIMER, START TIMER, and RESTART TIMER

#### Scenario: Device action unchanged shape
- **WHEN** the operator adds a device-target action
- **THEN** the row uses device, channel, and payload controls like Manual command

### Requirement: Appearance follows Consoles in the sidebar
The main sidebar MUST place Appearance immediately after Consoles. Automation MUST sit immediately after Devices. Relative order MUST be Devices, Automation, Security, Consoles, Appearance, System (other earlier tabs unchanged).

#### Scenario: Admin sidebar order
- **WHEN** an `isAdmin` views the gateway sidebar
- **THEN** Automation is after Devices and Appearance is after Consoles

## MODIFIED Requirements

### Requirement: Main navigation uses sidebar sections

The console MUST present main sections as a left sidebar strip (horizontal folder strip on a narrow viewport), including Status, WiFi, MQTT, ZigBee, Devices, Automation, Security, Consoles, Appearance, and System in the operator-visible set. Selecting a main section MUST show that section’s panel and hide the others without a full page reload. Status MUST show the live summary (counts, packet counters, version) without settings forms. Devices MUST show the registered-device table and add-device flow. Automation MUST show the automations list and edit flow.

#### Scenario: Switch main section

- **WHEN** the operator selects WiFi after Status was visible
- **THEN** the WiFi panel is shown and the Status panel is hidden

#### Scenario: Placeholder tabs have no forms

- **WHEN** the operator opens Status
- **THEN** the panel has no configuration controls and does not change device settings

#### Scenario: Open Automation

- **WHEN** an `isAdmin` selects Automation
- **THEN** the Automation panel is shown and other main panels are hidden

### Requirement: Sidebar and System tabs follow roles
The console MUST hide main sections and System folder tabs the signed-in user is not allowed to use. A simple user MUST NOT use the gateway sidebar. A user with operator roles and without `isAdmin` MUST see Status, Devices, System → Log, and System → Maintenance (Appearance only) plus any extra sections their roles allow. A user without `isAdmin` MUST NOT see WiFi, MQTT, ZigBee, Automation, System → Update, System → Hardware, or Maintenance Settings (export/restore) unless `isAdmin`. `editUsers` or `isAdmin` MUST show Security. `editConsoles` or `isAdmin` MUST show Consoles immediately after Security. When Consoles and Appearance are both visible, Appearance MUST follow Consoles. `isAdmin` MUST see every main section and every System tab.

#### Scenario: Simple user navigation
- **WHEN** a simple user is signed in
- **THEN** the gateway sidebar is not shown

#### Scenario: editUsers sees Security
- **WHEN** a user with `editUsers` and no `isAdmin` is signed in
- **THEN** the sidebar includes Security

#### Scenario: Console editor sees Consoles after Security
- **WHEN** a user with `editConsoles` and `editUsers` and no `isAdmin` is signed in
- **THEN** the sidebar shows Consoles immediately after Security

#### Scenario: Admin sees Consoles without editConsoles
- **WHEN** an `isAdmin` user with `editConsoles` unset is signed in
- **THEN** the sidebar shows Consoles immediately after Security

#### Scenario: Non-admin hides Automation
- **WHEN** a signed-in user has operator roles but not `isAdmin`
- **THEN** Automation is not shown in the sidebar

### Requirement: Maintenance exports settings without Wi-Fi
System → Maintenance SHALL show **Export settings** above **Restore settings**. Both SHALL use compact inline buttons, not full-width bars. Export SHALL download a JSON file that includes MQTT settings, Zigbee settings, hardware SPI speed and blue/green LED brightness, the signed-in user’s theme, the persisted device list (same device fields as Devices Export, no live telemetry), the users table, control consoles, and automations (persisted fields only, no last-activation or TIMER runtime telemetry). Each exported user SHALL include user name, added-at, roles including `editConsoles`, `consoles`, `isBlocked`, theme, and password **hash and salt**. The file SHALL NOT contain a plaintext user password. The file SHALL NOT contain a Wi-Fi group (no BSSID, password, MODE, AP IP, hostname, or OTG). Only `isAdmin` SHALL see or use Export settings and Restore settings.

#### Scenario: Export omits Wi-Fi
- **WHEN** the operator clicks Export settings and the host has Wi-Fi, MQTT, Zigbee, hardware, a saved theme, and at least one registered device
- **THEN** the downloaded JSON has mqtt, zigbee, hardware, ui theme, and devices, and has no wifi object or Wi-Fi password

#### Scenario: Export includes LED brightness
- **WHEN** the operator exports settings after saving blue and green brightness
- **THEN** `hardware` in the JSON includes `blueLedBrightness` and `greenLedBrightness`

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

### Requirement: Maintenance restores settings after confirm
Restore settings SHALL ask the operator to confirm before applying a file. After confirm, the host SHALL replace MQTT, Zigbee, hardware SPI speed, blue/green LED brightness, and the registered device list from the file. When `ui.theme` is present, the host SHALL apply it to the **signed-in user** only. When a users list is present, the host SHALL replace the users table from that list (hashes and salts, never plaintext passwords). When a consoles list is present, the host SHALL replace control consoles from that list. When an automations list is present, the host SHALL replace automations from that list. The host SHALL leave Wi-Fi unchanged even if the file contains a wifi object. A file that is not valid JSON, or that lacks a usable settings body, SHALL be rejected and SHALL NOT write settings. Only `isAdmin` SHALL restore settings. Devices Export/Restore on the Devices page SHALL remain. A restore file whose `hardware` object lacks brightness fields SHALL keep the host’s current brightness values.

#### Scenario: Confirm then apply
- **WHEN** the operator chooses Restore settings, selects a valid export file, and confirms
- **THEN** MQTT, Zigbee, hardware, theme for the signed-in user, devices, and users (when present in the file) match the file and Wi-Fi settings are unchanged

#### Scenario: Restore applies LED brightness
- **WHEN** the file’s hardware has `blueLedBrightness` 25 and `greenLedBrightness` 80 and the operator confirms
- **THEN** those brightness values are stored and applied on the host (and pushed to the slave when linked)

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
