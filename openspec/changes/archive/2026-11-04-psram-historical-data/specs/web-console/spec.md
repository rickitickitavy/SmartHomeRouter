# Spec Delta

## ADDED Requirements

### Requirement: Status Historical storage group
Status SHALL show a **Historical storage** group with entries count, pages count, PSRAM usage in kibibytes (1024-byte KiB), and values stored (total samples across all pages). Values SHALL come from the gateway status API. Status SHALL NOT offer history settings forms. Operator Status and simple-user Status MUST both show this group when the viewer can open Status.

#### Scenario: Counts visible
- **WHEN** Status is open and the host has 2 history entries, 5 pages, 20 KiB history slab usage, and 100 stored values
- **THEN** Historical storage shows those four figures

### Requirement: Hardware history retention and slab size
System → Hardware SHALL show Max History store days (1–20, default 7), History sweep interval minutes (1–120, default 10), Maximum history size in KiB (default **3072**), and **Entry size** as a hard choice of 256, 512, 1024, 2048, or 4096 (default 1024). `/api/hardware` SHALL round-trip these fields, reject invalid days/interval/entry size, and apply slab size and entry size at next boot. Only `isAdmin` SHALL use Hardware.

#### Scenario: Defaults on first open
- **WHEN** an admin opens Hardware and history settings were never saved
- **THEN** days show 7, sweep interval shows 10, Maximum history size shows 3072, and Entry size shows 1024

#### Scenario: Reject bad days
- **WHEN** POST `/api/hardware` sends Max History store days `0` or `21`
- **THEN** the host responds 400 and does not change stored history settings

#### Scenario: Reject bad entry size
- **WHEN** POST `/api/hardware` sends Entry size `300`
- **THEN** the host responds 400 and does not change stored Entry size

### Requirement: Device dialog Hist flag and epsilon
The device parameter dialog SHALL show Hist (square-only) and epsilon for main-state history. Stored epsilon `< 0` loads as Hist off with epsilon empty/disabled/grayed; Hist off keeps epsilon disabled. Save off persists `-1`; save on persists entered epsilon `>= 0`. Reportable rows SHALL use the same pattern; string rows are flag-only (`0` on / `-1` off).

#### Scenario: Load off state
- **WHEN** the operator opens a device whose history epsilon is `-1`
- **THEN** Hist is unchecked and the epsilon field is empty and grayed

#### Scenario: Save enable
- **WHEN** the operator checks Hist, enters epsilon `0.5`, and saves
- **THEN** the stored device epsilon is `0.5`

### Requirement: History chart UI for binding
When a user opens history from a console binding, the console SHALL show a Home Assistant–like history view for that bound series: numeric series as a step-held line chart, binary/state series as a colored state timeline. The same UI SHALL be available on the operator page and the simple-user page for users who can view that console.

#### Scenario: Binary state band
- **WHEN** the user opens history for a historized on/off binding
- **THEN** the view shows a colored state timeline for that series rather than a continuous sensor line

## MODIFIED Requirements

### Requirement: Status groups use compact name-value rows
Status SHALL render live attributes as groups on a two-column Status stack (one column when the viewport is narrow). Column 1 SHALL place online time, then **Version**, then **Network**, then **Connections**. Column 2 SHALL place **Free memory** at the top, then **Historical storage**, then **MQTT**, then **Devices** after MQTT, then **WiFi** at the end. Each group SHALL have a heading. Each attribute SHALL keep its name and value on a single row. Values in a group SHALL align to the right. Groups SHALL use a mindful width (content plus padding) and SHALL NOT stretch to the full main-panel width. Status SHALL NOT contain settings forms that write configuration.

#### Scenario: Grouped Status layout
- **WHEN** the operator opens Status on a wide viewport
- **THEN** column 1 shows online time, Version, Network, and Connections, and column 2 shows Free memory, Historical storage, MQTT, Devices, then WiFi, each group narrower than the main panel, with names on the left and values on the right of the same row

### Requirement: Hardware tab sets blue and green LED brightness
System → Hardware SHALL show SPI speed plus **BLUE LED brightness** and **GREEN LED brightness** as integer fields from 1 to 100 (percent), a checkbox **use queue for base device command** (square-only toggle; default unchecked / false), plus the history fields **Max History store days**, **History sweep interval (minutes)**, **Maximum history size** (kB), and **Entry size** (hard choice 256/512/1024/2048/4096). GET `/api/hardware` SHALL return `spiSpeedHz`, `blueLedBrightness`, `greenLedBrightness`, `useQueueForBaseDeviceCommand` (boolean), and the history fields. POST `/api/hardware` SHALL require the brightness and SPI fields as today, accept the boolean (omitted on older clients MUST mean false), accept the history fields (omitted on older clients MUST keep prior stored values or factory defaults), reject values outside 1–100 for either brightness, reject out-of-range history days or sweep interval, reject Entry size values outside the hard choice set, persist them with hardware settings, apply brightness without requiring a reboot, push brightness to the slave when the link is up, and persist the queue flag for use by the action/command FIFO. Factory default for both brightness fields SHALL be 100. Factory default for **use queue for base device command** SHALL be false. Factory defaults for history SHALL be days 7, sweep 10 minutes, Entry size 1024, and Maximum history size **3072** KiB. Only `isAdmin` SHALL use Hardware.

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
