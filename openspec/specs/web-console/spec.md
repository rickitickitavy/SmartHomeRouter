# web-console Specification

## Purpose

Gives this Zigbee–MQTT gateway a browser console on its own IP so an operator can open a tabbed UI, set Wi-Fi AP/STA, flash firmware, and read recent logs without the USB CLI.

## Requirements

### Requirement: Console is reachable over HTTP

The firmware MUST serve an HTTP web console on port 80 whenever Wi-Fi AP or STA has an address. Opening the device root URL MUST return the console page (login form when there is no session, signed-in chrome when there is a valid session). The USB CLI, MQTT, and Zigbee control paths MUST keep working while the console is served. Protected console APIs MUST require a valid session.

#### Scenario: Open console on AP

- **WHEN** the gateway is in AP mode and a client opens `http://192.168.0.1/`
- **THEN** the browser receives the web console HTML

#### Scenario: Open console on STA

- **WHEN** the gateway has a STA address and a client opens `http://<sta-ip>/`
- **THEN** the browser receives the same web console HTML

#### Scenario: Wi-Fi is down

- **WHEN** neither AP nor STA is up
- **THEN** the console is not reachable on the LAN and existing USB CLI behavior is unchanged

### Requirement: Console stays reachable after STA address refresh
When the host is in STA mode and the station obtains an IPv4 address after a disconnect, roam, or DHCP renew, the HTTP web console on port 80 MUST answer on that current STA address. ICMP reachability and MQTT client traffic MUST NOT be treated as proof that the console is already bound. The first HTTP bind after a successful boot join MUST remain the bind used until a later STA address event; that first bind MUST NOT be torn down immediately. USB CLI, MQTT, and Zigbee/SPI control MUST keep working while the console listen socket is rebound.

#### Scenario: Console answers after STA reconnect
- **WHEN** STA drops and later has an IPv4 address again
- **THEN** opening `http://<sta-ip>/` returns the web console HTML without requiring a device reboot

#### Scenario: Console answers after STA GOT_IP without a polled disconnect
- **WHEN** STA reports a new or renewed IPv4 address while ICMP and MQTT still work
- **THEN** opening `http://<sta-ip>/` returns the web console HTML

#### Scenario: First boot bind is not rebound
- **WHEN** the host has just bound the console after the boot STA join succeeded
- **THEN** the console remains that first listen and is not immediately closed and opened again

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

### Requirement: Automation section lists automations
The Automation section MUST show a narrow left group list and a table of automations. The group list MUST start with **All**, then distinct non-empty groups, and include **Not in group** when any ungrouped automation exists (or always as a selectable bucket). Selecting **All** MUST show every automation. Selecting a named group MUST show only automations with that `group`. Selecting **Not in group** MUST show only automations with empty `group`. The table MUST show at least name, active, and last performed when known, plus timer columns already present. Buttons Add, Export all, and Import all remain. Only `isAdmin` MUST see Automation.

#### Scenario: Filter by group
- **WHEN** an `isAdmin` selects group `Lights` and two automations have that group
- **THEN** the table shows only those two rows

#### Scenario: All shows everything
- **WHEN** an `isAdmin` selects **All**
- **THEN** the table includes automations from every group and ungrouped ones

#### Scenario: Open Automation list
- **WHEN** an `isAdmin` opens Automation and at least one automation is stored
- **THEN** each visible row shows name, active, and last performed when known

### Requirement: Automation edit dialog import and export
Add or row edit MUST open an edit dialog with **General** (Active, then Name, then Group, Debug; no title), **Trigger** (countdown TIMER controls first, then listening devices with typed parameter pickers), Condition (enable checkbox plus editor when enabled, with typed comparison pickers), actions (device actions with typed pickers or automation targets), Export, and Import. Dialog Import MUST be enabled only when the form is empty (no listeners, no actions, condition disabled or empty, TIMER `use` false / defaults unset). Dialog Export MUST download one JSON file named from the automation’s `name`.

#### Scenario: Group after name
- **WHEN** the operator opens the automation edit dialog
- **THEN** the Group field appears immediately below Name and accepts typed text or a choice from existing groups

#### Scenario: Empty-only dialog import
- **WHEN** the edit dialog already has a listener, action, or TIMER `use` true
- **THEN** Import is disabled

#### Scenario: No Title field
- **WHEN** the operator opens the automation edit dialog
- **THEN** there is no Title input

### Requirement: Automation Trigger rows use typed parameters
Each Trigger (listening) row MUST let the operator choose a registered device, a channel when that device has more than one channel (channel control MUST be hidden when only one channel is available), and either a **known parameter** for that device’s classified type or **CUSTOM**. When CUSTOM is selected, the row MUST show cluster and attrId fields for entry (hex). The Remove control on Trigger rows MUST use the same compact button sizing as Remove controls in Condition and Actions (`btn-inline` / shared editor button style).

#### Scenario: Hide single-channel picker
- **WHEN** the operator selects a device that has only one channel on a Trigger row
- **THEN** no channel control is shown for that row

#### Scenario: Custom shows cluster and attrId
- **WHEN** the operator chooses CUSTOM on a Trigger row
- **THEN** cluster and attrId inputs are visible and required for a complete custom binding

#### Scenario: Trigger Remove matches other editors
- **WHEN** the operator views Trigger, Condition, and Actions Remove buttons
- **THEN** Trigger Remove uses the same compact inline button styling as the other Remove buttons

### Requirement: Automation Condition comparisons use typed parameters
Each condition comparison MUST use the same device / channel / known-parameter-or-CUSTOM picker as Trigger. For a known parameter, the compare value MUST be chosen or entered against the host’s interpreted status vocabulary for that parameter. For CUSTOM, GREATER/LESS MUST apply to numeric VAL; EQUAL MAY use interpreted status text (for example DRY) or a numeric value.

#### Scenario: Custom condition uses VAL
- **WHEN** the operator saves a CUSTOM condition comparison with GREATER and value `42`
- **THEN** evaluation compares the numeric attribute VAL to 42

#### Scenario: Custom EQUAL to status text
- **WHEN** the operator saves a CUSTOM condition comparison with EQUAL and value `DRY`
- **THEN** the dialog stores that binding and the host accepts it

#### Scenario: Known On/Off status compare
- **WHEN** the operator picks an On/Off device’s Status parameter and EQUAL to ON
- **THEN** the dialog stores a known Status binding that compares the interpreted ON/OFF status

### Requirement: Automation device actions use typed parameters
Each device-target action MUST use the same device / channel / known-parameter-or-CUSTOM picker pattern as Trigger. Known parameters MUST map to the existing device-command payloads for that type (for example On/Off ON/OFF, window covering open/close/stop/position). When CUSTOM is selected, the row MUST show cluster, command, and value fields (cluster and command as hex).

#### Scenario: Custom action fields
- **WHEN** the operator chooses CUSTOM on a device action
- **THEN** cluster, command, and value inputs are visible

#### Scenario: Known dimmer light level
- **WHEN** the operator picks Light level on a level-capable device action
- **THEN** the dialog collects the level value and stores a known Light-level action binding

### Requirement: Known parameter lists by device type
The console MUST offer known parameters based on the device’s classified type (and related clusters when present), including at least: Window covering — Status (STOPPED, UP, DOWN) and Position; On/Off — Status (ON, OFF); Level/dimmer — Status (ON, OFF) and Light level; measurement sensors — Sensor value (and Battery level when the device exposes battery); IAS zone — zone Status (LEAK/DRY style strings the host already publishes); User data — each logical name from that device’s persisted `userDataFields` in schema order. Devices with no matching catalog entry MUST still offer CUSTOM. For `userData`, CUSTOM remains available in addition to the named fields.

#### Scenario: Window covering known list
- **WHEN** the operator selects a window-covering device on Trigger or Condition
- **THEN** the parameter list includes Status and Position plus CUSTOM

#### Scenario: User data named fields
- **WHEN** the operator selects a `userData` device whose schema has fields `temp` and `label` on Trigger, Condition, Actions, or a console binding param picker
- **THEN** the parameter list includes `temp` and `label` (in that order) plus CUSTOM

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

### Requirement: Devices card lists registered devices and can add one
The Devices section SHALL show a table of registered devices (at least IEEE, friendly name, last RSSI when known) and Add device, Export, and Restore. Opening Devices SHALL load the registered list from the host. After a successful parameter Save, the table SHALL include the new row without requiring a full page reload.

#### Scenario: Open Devices
- **WHEN** the operator opens the Devices section and the host has at least one registered device
- **THEN** that device appears in the table

#### Scenario: Empty map
- **WHEN** the operator opens Devices and no devices are registered
- **THEN** the table is empty and Add device is still available

### Requirement: Add device uses a search dialog then a parameter dialog

Add device SHALL open a search dialog that contains a table of found-but-unregistered devices, a Search control, and an MQTT DEVICE control. Opening the dialog SHALL NOT start pairing. Search SHALL start pairing on the slave only when clicked and SHALL update the found table as the host learns new joins. Add SHALL be enabled only when one found row is selected. Add SHALL close the search dialog and open a parameter dialog for that Zigbee device with transport `zigbee`. MQTT DEVICE SHALL open a parameter dialog for a new `mqtt` device without requiring a found row. The Zigbee parameter dialog SHALL show IEEE (not editable) and SHALL accept friendly name and MQTT topic fields. The MQTT create dialog SHALL accept editable IEEE, friendly name, MQTT topic fields, and cluster type. Save SHALL persist the device and close the parameter dialog. Dismissing the search dialog SHALL stop pairing search.

#### Scenario: Search fills the found table

- **WHEN** the search dialog is open, the operator clicks Search, and an unregistered device joins
- **THEN** that device appears in the found table without closing the dialog

#### Scenario: Add then Save

- **WHEN** the operator selects a found device, clicks Add, fills the parameter form, and clicks Save
- **THEN** the dialogs are closed and the Devices table shows the new registered device with transport `zigbee`

#### Scenario: Cancel search

- **WHEN** the operator closes the search dialog without Add
- **THEN** no new registered device is written and pairing search stops

#### Scenario: WIFI DEVICE then Save

- **WHEN** the operator clicks MQTT DEVICE, fills IEEE, name, and topics, and saves
- **THEN** the Devices table shows the new registered device with transport `mqtt`

### Requirement: ZigBee tab edits the Zigbee group

The ZigBee tab MUST show and allow saving CHANNEL and PERMIT_JOIN_ON_BOOT_SEC from the Zigbee group record. CHANNEL MUST be 11–26. PERMIT_JOIN_ON_BOOT_SEC MUST be 0–254. The page MUST load those values from `GET /api/zigbee` when the tab opens and on first page load, and MUST persist them with `POST /api/zigbee`. Saving MUST write EEPROM and apply the radio settings without restarting the router. Reset MUST reload stored values and MUST NOT write settings.

#### Scenario: Load Zigbee settings

- **WHEN** the operator opens the ZigBee section
- **THEN** CHANNEL and PERMIT_JOIN_ON_BOOT_SEC show the values stored on the host

#### Scenario: Save Zigbee settings

- **WHEN** the operator sets a valid channel and permit-join duration and saves
- **THEN** the Zigbee group is persisted and the radio settings are applied without a router restart

### Requirement: Settings are stored in groups

Persistent settings MUST live in `GlobalSettings` as separate group records. Each functional group MUST be one record. The Wi-Fi group record MUST contain BSSID, PASSWORD, DEVICE_NAME, AP IP, MODE, and OTG_ENABLED. Default MODE MUST be AP. Default BSSID MUST be `z2m-gateway` and MUST be the network name for both STA join and AP advertise. Default PASSWORD MUST be `00000000` for both AP and STA. Default AP IP MUST be `192.168.0.1` and MUST apply only when the radio is in AP mode. Default OTG_ENABLED MUST be false. DEVICE_NAME MUST be used only as the Wi-Fi hostname and MUST NOT be used as the AP or STA network name. Other existing setting fields MUST be placed in their own group records without changing their meaning in this change.

#### Scenario: Defaults on first boot

- **WHEN** settings are uninitialized and defaults are written
- **THEN** the Wi-Fi group has MODE AP, OTG_ENABLED false, PASSWORD `00000000`, AP IP `192.168.0.1`, BSSID `z2m-gateway`, and a default DEVICE_NAME used only as hostname

#### Scenario: Wi-Fi group is one record

- **WHEN** the firmware reads or writes Wi-Fi configuration
- **THEN** BSSID, PASSWORD, DEVICE_NAME, AP IP, MODE, and OTG_ENABLED are stored together in the Wi-Fi group record

### Requirement: WiFi tab edits the Wi-Fi group

The WiFi tab MUST show and allow saving BSSID, PASSWORD, DEVICE_NAME, AP IP, MODE (AP or STA), and OTG_ENABLED. AP IP MAY be hidden while MODE is STA. Saving MUST persist the Wi-Fi group and apply the boot rules on the next Wi-Fi start (restart is allowed). BSSID is one network-name string for STA and AP (not a MAC address). DEVICE_NAME MUST NOT be written to MQTT or Zigbee settings.

#### Scenario: Save STA settings

- **WHEN** the operator sets MODE to STA, enters router BSSID and PASSWORD, and saves
- **THEN** the Wi-Fi group is persisted with those values and MODE STA

#### Scenario: Save AP settings

- **WHEN** the operator sets MODE to AP, enters BSSID and PASSWORD, and saves
- **THEN** the Wi-Fi group is persisted with those values and MODE AP

### Requirement: Wi-Fi boot mode and AP naming

If BSSID is empty at boot, the device MUST start in AP mode using the default BSSID `z2m-gateway`. If MODE is STA and BSSID is set, the device MUST try to join that network for 5 seconds. If the join does not succeed in that window, the device MUST switch to AP mode using the same BSSID and MUST remain in AP until the next boot or a settings apply. The AP IPv4 address MUST be the stored AP IP (default `192.168.0.1`). PASSWORD MUST be the PSK for both STA join and AP (default `00000000`). DEVICE_NAME MUST be applied only as the Wi-Fi hostname. OTG_ENABLED MUST be persisted and shown; it MUST NOT change radio mode in this change.

#### Scenario: Unconfigured starts AP

- **WHEN** the device boots with default Wi-Fi settings (MODE AP, BSSID `z2m-gateway`)
- **THEN** it starts AP and does not attempt STA

#### Scenario: STA connects within 5 seconds

- **WHEN** the device boots with MODE STA and a configured BSSID and the router accepts the join within 5 seconds
- **THEN** the device stays in STA with a LAN address

#### Scenario: STA fails then AP uses the same BSSID

- **WHEN** the device boots with MODE STA and a configured BSSID and the join does not succeed within 5 seconds
- **THEN** the device switches to AP and the AP network name is that same BSSID

#### Scenario: Configured AP uses BSSID

- **WHEN** the device boots with MODE AP and BSSID `z2m-gateway`
- **THEN** it starts AP whose network name is `z2m-gateway`

### Requirement: System has Update and Log tabs

When System is selected, the console MUST present nested folder-style tabs in this order: Log, devices.json, Update, Hardware. The default System panel MUST be Log. Selecting a tab MUST show that panel and hide the others without a full page reload.

#### Scenario: Nested System tabs

- **WHEN** the operator opens System and then selects Log
- **THEN** the Log panel is shown and the Update panel is hidden

#### Scenario: Default System tab is Log
- **WHEN** the operator opens System
- **THEN** the Log tab is selected first

### Requirement: Firmware update uploads one package for both chips
The Update tab MUST show the running firmware version and accept a **joined firmware package** upload over HTTP (ZIP containing `slave.bin` and `host.bin`). After a valid file is received, the host MUST program the slave from `slave.bin` over SPI first. Only after the slave reports a successful firmware commit MUST the host program itself from `host.bin` and restart. A failed HTTP receive, invalid package, failed slave transfer, or failed slave commit MUST leave the host’s running application unchanged, MUST NOT restart the host, and MUST show a failure on the page.

#### Scenario: Successful firmware upload
- **WHEN** the operator uploads a valid joined package from the Update tab and the slave commit succeeds
- **THEN** the host applies its image and restarts; both chips run the new firmware

#### Scenario: Failed firmware upload
- **WHEN** the upload is rejected or staging fails
- **THEN** the currently running firmware remains unchanged and the page reports the failure

#### Scenario: Slave OTA fails
- **WHEN** the joined package is accepted but the slave transfer or slave commit fails
- **THEN** the host does not apply its image, does not restart for update, and the page reports the failure

### Requirement: Firmware Update shows slave-then-host progress

After the browser finishes sending the firmware file, the Update card SHALL show that the gateway is updating the slave (phase label **RF firmware update**), then the host, until success or failure. The HTTP file-upload progress bar alone SHALL NOT be treated as a completed firmware update. The slave-progress phase label MUST be **RF firmware update** and MUST NOT be **Master to slave**.

#### Scenario: Status after file lands

- **WHEN** the firmware file has finished uploading over HTTP and the slave is still being programmed
- **THEN** the Update card shows that the RF firmware update is in progress and does not claim the device has already restarted

#### Scenario: Phase percents

- **WHEN** firmware update is in progress
- **THEN** the Update card shows upload-to-master percent, RF firmware update percent, and host-apply percent together with the current phase

### Requirement: Firmware Update warns before host reboot

When the host is about to restart after a successful firmware write, the Update card SHALL show a warning that the router will reboot, SHALL count down a 30 second wait, and SHALL ping the router in the background until it responds or the wait expires.

#### Scenario: Router returns within wait

- **WHEN** the host restarts after firmware apply and `/api/version` succeeds within 30 seconds
- **THEN** the Update card reports that the update finished successfully

#### Scenario: Router wait times out

- **WHEN** the host does not answer `/api/version` within 30 seconds after the reboot warning
- **THEN** the Update card reports a wait timeout error

### Requirement: Console HTML and CSS ship inside the firmware image

The host MUST serve the web console HTML and CSS from assets that are part of the application firmware image for the running build. The console page and styles MUST NOT depend on a separately flashed LittleFS web image. Opening `/` or `/index.html` MUST return the console HTML for that firmware version. Requests under `/css/` MUST return the matching stylesheet from the same image.

#### Scenario: Root after firmware-only flash

- **WHEN** the operator has applied a firmware image that includes the console and has not flashed a separate web filesystem image
- **THEN** opening `http://<device-ip>/` returns the console HTML for that firmware version

#### Scenario: Stylesheet from the same image

- **WHEN** the signed-in console page requests its stylesheet under `/css/`
- **THEN** the browser receives the CSS that belongs to that same firmware build

### Requirement: Firmware upload alone updates the console UI

A successful firmware OTA (existing slave-then-host flow) MUST be sufficient to update both the application and the web console UI. The System → Update tab MUST NOT offer a separate LittleFS or web-filesystem upload as the way to refresh console HTML/CSS.

#### Scenario: Update tab has firmware only for UI

- **WHEN** the operator opens System → Update
- **THEN** the page shows the firmware upload path and does not show a filesystem upload control for refreshing the console UI

#### Scenario: New UI after firmware OTA

- **WHEN** the operator completes a successful firmware update that embeds newer console assets
- **THEN** the next full page load of the console shows that newer HTML/CSS without a separate filesystem flash

### Requirement: MQTT card shows stored settings

When the operator opens the MQTT section, each MQTT field SHALL display the value currently stored on the host, including SERVER BORN TOPIC and BORN INTERVAL. Empty stored fields MAY stay empty; the form SHALL NOT stay blank when the device has MQTT settings.

#### Scenario: Open MQTT with saved server

- **WHEN** the host has a stored MQTT server and the operator opens the MQTT card
- **THEN** the server field (and the other MQTT fields, including born topic and interval) show the stored values

### Requirement: MQTT SERVER TYPE replaces ENABLED
The MQTT section MUST show SERVER TYPE as a three-option control (`disable`, `remote`, `local`) and MUST NOT show an ENABLED checkbox. GET `/api/mqtt` MUST return `serverType` as one of those strings. POST `/api/mqtt` MUST persist `serverType` and MUST NOT require a boolean `enabled`. Factory default SERVER TYPE MUST be `disable`. Settings export and restore MUST include `mqtt.serverType`. Restore of a file that still has `mqtt.enabled` and no `serverType` MUST map `false` to `disable` and `true` to `remote`.

#### Scenario: Open MQTT card
- **WHEN** the operator opens the MQTT section
- **THEN** SERVER TYPE shows the stored value and ENABLED is not shown

#### Scenario: Save local
- **WHEN** the operator sets SERVER TYPE to `local` and saves
- **THEN** GET `/api/mqtt` returns `"serverType":"local"`

#### Scenario: Restore legacy enabled
- **WHEN** an admin restores a file whose MQTT object has `"enabled":true` and no `serverType`
- **THEN** stored SERVER TYPE is `remote`

### Requirement: SERVER field hidden when type is local
When SERVER TYPE is `local`, the MQTT SERVER field MUST be hidden and POST `/api/mqtt` MUST NOT require `server`. When SERVER TYPE is `disable` or `remote`, the SERVER field MUST be visible. Empty SERVER on save in those modes MAY keep the stored default host string.

#### Scenario: Local hides SERVER
- **WHEN** the operator selects SERVER TYPE `local`
- **THEN** the SERVER field is not shown

#### Scenario: Remote shows SERVER
- **WHEN** the operator selects SERVER TYPE `remote`
- **THEN** the SERVER field is shown

### Requirement: WiFi card scrolls and keeps Save and Reset reachable

The WiFi section SHALL scroll when its fields do not fit the main panel. Save and Reset SHALL remain usable (visible after scroll or pinned at the bottom of the card). Reset SHALL reload the form from the device without writing new settings.

#### Scenario: Tall WiFi card

- **WHEN** the WiFi fields do not fit the viewport
- **THEN** the operator can scroll the WiFi card and reach Save and Reset

#### Scenario: Reset reloads WiFi

- **WHEN** the operator changes a WiFi field and then clicks Reset
- **THEN** the fields return to the last stored values from the device

### Requirement: Log tab shows recent firmware logs

The Log tab MUST display recent firmware log lines that are also emitted on USB when console logging is enabled. The page MUST be able to refresh that list without leaving the Log tab. Older lines MAY drop when the in-memory buffer is full. The Refresh control MUST stay visible without a second page scroller hiding it, MUST be left-aligned, and MUST use a compact content width (not a full-width bar). The Log card MUST NOT repeat a second “Log” title under the Log tab.

#### Scenario: New log line appears

- **WHEN** the firmware emits a log line and the operator refreshes the Log tab (automatically or manually)
- **THEN** that line appears in the log view (unless it has already aged out of the buffer)

#### Scenario: USB logging still works

- **WHEN** a log line is emitted
- **THEN** it still appears on the USB serial console when USB debug logging is enabled

#### Scenario: Refresh stays visible

- **WHEN** the operator opens System → Log
- **THEN** Refresh is visible, left-aligned, and not stretched across the card

#### Scenario: No duplicate Log title
- **WHEN** the operator opens System → Log
- **THEN** the tab is labeled Log and the card does not show another Log heading

### Requirement: Escape dismisses overlay dialogs

The console SHALL treat Escape as dismiss for every overlay dialog (search, device parameters, restore-skipped list, and delete confirm). Escape SHALL run the same dismiss path as that dialog’s Cancel or Close control: it MUST NOT save, MUST NOT confirm delete, and dismissing the search dialog MUST stop pairing search. When more than one overlay is visible, Escape SHALL dismiss only the front-most overlay. Escape SHALL do nothing when no overlay is visible.

#### Scenario: Escape closes search

- **WHEN** the search dialog is visible and the operator presses Escape
- **THEN** the search dialog closes, pairing search stops, and no new registered device is written

#### Scenario: Escape cancels delete

- **WHEN** the delete-confirm dialog is visible and the operator presses Escape
- **THEN** the confirm dialog closes and the selected device remains registered

#### Scenario: Escape ignored with no overlay

- **WHEN** no overlay dialog is visible and the operator presses Escape
- **THEN** the page does not navigate away and no dialog opens or closes

### Requirement: Device parameter dialog includes FULL CONTROL
The Devices parameter dialog SHALL include a FULL CONTROL checkbox as the last setting field (after IEEE, name, channels, and the MQTT topic fields). The control SHALL load from the stored record and SHALL be unchecked when the flag is omitted or off. Save SHALL persist the checkbox with the device. Binary device settings SHALL use a checkbox, not a text field or select of on/off strings.

#### Scenario: New device shows off at the end
- **WHEN** the operator opens the parameter dialog for a newly added device
- **THEN** FULL CONTROL is unchecked and appears after AVAILABILITY

#### Scenario: Load stored on
- **WHEN** the operator opens parameters for a device stored with FULL CONTROL on
- **THEN** the FULL CONTROL checkbox is checked and still last in the form

#### Scenario: Save turns it on
- **WHEN** the operator checks FULL CONTROL and saves
- **THEN** the stored record has FULL CONTROL on

### Requirement: NAME edits rewrite default MQTT topics
While the device parameter dialog is open, each change to NAME SHALL rewrite the state, command, and availability fields to the default topics for that name and the stored MQTT base topic (`{base}/{slug}/state`, `{base}/{slug}/set`, `{base}/{slug}/availability`). This SHALL apply when adding and when editing an existing device.

#### Scenario: Rename rewrites topics
- **WHEN** the operator edits NAME from `switch_1` to `kitchen_lamp`
- **THEN** the three topic fields update to the default pattern that uses `kitchen_lamp`

### Requirement: Double-click opens device edit
A single click on a registered-device table row SHALL select that IEEE and open the parameter dialog for that IEEE, same as Edit. A double-click SHALL NOT be required.

#### Scenario: Double-click row
- **WHEN** the operator single-clicks a registered device row
- **THEN** that row is selected and the edit parameter dialog opens for that device

### Requirement: Devices Export downloads the slave store
Beneath the registered-device table the console SHALL provide Export. Export SHALL download the slave devices store JSON (the same document as System devices.json), for later Restore.

#### Scenario: Export file
- **WHEN** the operator clicks Export
- **THEN** the browser downloads the current slave `devices.json` content

### Requirement: Devices Restore matches System restore
Beneath the registered-device table the console SHALL provide Restore that uses the same file-pick and restore path as System devices.json Restore (one device record at a time to the slave).

#### Scenario: Restore from Devices
- **WHEN** the operator chooses Restore on Devices and selects a valid devices JSON array
- **THEN** missing devices are restored the same way as System devices.json Restore

### Requirement: Search follows the slave join window
Opening the pairing dialog SHALL NOT start pairing on the slave. Search SHALL start the slave join window only when the operator clicks Search. While that window is open, Search SHALL stay disabled. When the slave join window ends, Search SHALL become enabled again even if the dialog stays open. Closing the dialog SHALL still stop pairing.

#### Scenario: Open does not search
- **WHEN** the operator opens Add device
- **THEN** the pairing dialog is visible and the slave join window is not started

#### Scenario: Search until slave ends
- **WHEN** the operator clicks Search and the slave join window later closes
- **THEN** Search is enabled again without requiring the dialog to close

### Requirement: Devices table shows last packet RSSI
The registered-device table SHALL include last received signal strength for each IEEE when the host has stored RSSI from the last inbound Zigbee packet for that device. A device with no received packet yet SHALL show an empty signal cell.

#### Scenario: RSSI after report
- **WHEN** the host has received a packet for a registered IEEE with RSSI −62 dBm
- **THEN** that row shows −62 dBm (or an equivalent dBm display of that value)

### Requirement: Status shows counts, traffic, and version
The Status section SHALL show host online time, master and slave firmware versions, registered device count, online device count, packets sent, packets received, MQTT local status, MQTT active topic count, MQTT connection count, Wi-Fi mode, BSSID, and Wi-Fi signal strength. Status SHALL obtain these values from the gateway status API. Status SHALL NOT contain settings forms that write configuration.

#### Scenario: Open Status with devices
- **WHEN** the operator opens Status and three devices are registered with one online
- **THEN** Status shows device count 3, online count 1, master firmware version from the running image, slave firmware version from the last slave report, and the packet counters

### Requirement: Status groups use compact name-value rows
Status SHALL render live attributes as groups in this order: online time (one row, not inside another group), then **Version**, then **Devices**, then **MQTT**, then **WiFi**. Each group SHALL have a heading. Each attribute SHALL keep its name and value on a single row. Values in a group SHALL align to the right. Groups SHALL use a mindful width (content plus padding) and SHALL NOT stretch to the full main-panel width. Status SHALL NOT contain settings forms that write configuration.

#### Scenario: Grouped Status layout
- **WHEN** the operator opens Status
- **THEN** the panel shows online time, then Version, Devices, MQTT, and WiFi groups, each narrower than the main panel, with names on the left and values on the right of the same row

### Requirement: Status online time includes days
Status SHALL show host online time as one value on one row. The value SHALL include days, hours, minutes, and seconds derived from host uptime seconds.

#### Scenario: Multi-day uptime
- **WHEN** the host has been up for 90061 seconds
- **THEN** Status online time shows 1 day, 1 hour, 1 minute, and 1 second (any compact `d h m s` wording)

### Requirement: Status Version group
The Version group SHALL show master firmware version and slave firmware version.

#### Scenario: Both versions
- **WHEN** Status is open and the host image is `0.2.21` and the slave last reported `0.2.21`
- **THEN** Version shows master `0.2.21` and slave `0.2.21`

### Requirement: Status Devices group
The Devices group SHALL show registered device count, online device count, packets sent, and packets received.

#### Scenario: Device counters
- **WHEN** three devices are registered, one is online, and the host has sent 4 packets and received 9
- **THEN** Devices shows count 3, online 1, packets sent 4, and packets received 9

### Requirement: Status MQTT group
The MQTT group SHALL show current mode (`disable`, `remote`, or `local`), runtime status, active MQTT topic count, and MQTT connection count. Runtime status SHALL be `connected` or `not connected` when mode is remote, `listening` or `off` when mode is local, and `off` when mode is disable. Connection count SHALL include remote LAN clients connected to the onboard broker when it is listening, plus the host client when it is connected.

#### Scenario: Local broker with a remote client
- **WHEN** SERVER TYPE is local, the onboard broker is listening, the host client is connected, and one other MQTT client is connected
- **THEN** MQTT shows current mode local, status listening, a topic count, and connections 2

#### Scenario: Broker unused
- **WHEN** SERVER TYPE is disable
- **THEN** MQTT shows current mode disable, status off, and connections is 0

#### Scenario: Remote disconnected
- **WHEN** SERVER TYPE is remote and the host client is not connected
- **THEN** MQTT shows current mode remote and status not connected

### Requirement: Status WiFi group
The WiFi group SHALL show Wi-Fi mode, BSSID (the same network name field as the WiFi card), and Wi-Fi signal strength. When the host is STA-associated, signal strength SHALL be the current STA RSSI. When there is no STA link, signal strength SHALL show an empty or placeholder value, not a stale STA RSSI.

#### Scenario: STA signal
- **WHEN** the host is in STA mode and associated with RSSI −58 dBm and BSSID `home-net`
- **THEN** WiFi shows mode STA, BSSID `home-net`, and signal −58 dBm

#### Scenario: AP without STA
- **WHEN** the host is in AP mode and is not STA-associated
- **THEN** WiFi shows mode AP and a placeholder for signal strength

### Requirement: Pairing found table shows readable device type
The pairing search found table SHALL show a readable device type for each found row. Labels SHALL be: `unknown` → Unknown; `onOff` → On/Off; `iasZone` → IAS Zone; `windowCovering` → Window covering. Manufacturer and model MAY still appear. The type SHALL come from the found-device API, not from the operator.

#### Scenario: Switch appears in Search
- **WHEN** Search is open and an unregistered `onOff` device joins
- **THEN** that row shows On/Off

### Requirement: Device parameters show readonly type

For a `zigbee` device, the device parameter dialog SHALL show the cluster device type as a readonly field. Add from Search SHALL show the found type. Edit of a registered `zigbee` device SHALL show the stored type. Save of a `zigbee` device SHALL persist other editable fields and SHALL NOT take a new type from an editable control. For an `mqtt` device, the cluster device type SHALL be an editable control among Unknown, On/Off, IAS Zone, Window covering, and User data, and Save SHALL persist the chosen type.

#### Scenario: Add shows found type

- **WHEN** the operator Adds a found `iasZone` device
- **THEN** the parameter dialog shows IAS Zone and the operator cannot edit that field

#### Scenario: Edit keeps stored type

- **WHEN** the operator opens parameters for a registered `zigbee` `onOff` device and saves a new name
- **THEN** the type remains `onOff` and the dialog still shows On/Off

#### Scenario: WiFi type editable

- **WHEN** the operator opens parameters for a registered `mqtt` device
- **THEN** the cluster type control is editable and Save can change the stored type

#### Scenario: MQTT User data selectable

- **WHEN** the operator opens parameters for a registered `mqtt` device
- **THEN** the type control includes User data and Save can store type `userData`

### Requirement: Devices table column order
The registered-device table SHALL show columns in this order: online, name, type, status, battery, RSSI. The table SHALL NOT show an IEEE column. IEEE SHALL remain visible on the device parameter dialog and SHALL remain the row identity for selection, edit, delete, and Manual command.

#### Scenario: Column order
- **WHEN** the operator opens Devices and at least one device is registered
- **THEN** the table headers are online, name, type, status, battery, RSSI and there is no IEEE header

### Requirement: Devices table shows battery percent or N/A
The battery cell SHALL show an integer percentage when the registered list JSON includes a battery percent for that IEEE. When that value is absent, the cell SHALL show `N/A`. Battery is live telemetry: the console SHALL NOT present it as an editable device setting.

#### Scenario: Percent after report
- **WHEN** the list JSON for a device includes battery `67`
- **THEN** that row’s battery cell shows `67`

#### Scenario: No battery yet
- **WHEN** the list JSON for a device omits battery
- **THEN** that row’s battery cell shows `N/A`

### Requirement: Devices table shows type-specific status
The status cell SHALL show the last known state for the device type. For `onOff`, each channel SHALL be a small circle: dark for OFF and light for ON. For other types, each channel SHALL show the last report text for that endpoint. When `channels` is `1`, the cell SHALL show one indicator. When `channels` is `0` or `2` through `16`, the cell SHALL show one indicator per channel, bound to inbound reports by the packet `ep`. A channel with no report yet SHALL show an empty or unknown indicator, not a guessed ON. After a boot status read, each `onOff` channel SHALL show the ON or OFF reported for that same endpoint.

#### Scenario: Single-channel on/off on
- **WHEN** a registered `onOff` device has `channels` `1` and last state ON
- **THEN** that row’s status cell shows one light circle

#### Scenario: Parse-mode two endpoints
- **WHEN** a registered `onOff` device has `channels` `0`, endpoint 1 last reported OFF, and endpoint 3 last reported ON
- **THEN** that row’s status cell shows a dark circle for endpoint 1 and a light circle for endpoint 3

#### Scenario: IAS zone text
- **WHEN** a registered `iasZone` device last reported `LEAK` on its status endpoint
- **THEN** that row’s status cell shows `LEAK`

#### Scenario: Four-channel relay after boot
- **WHEN** a registered `onOff` device has `channels` `4` and the boot read reported ON on endpoints 1 and 3 and OFF on endpoints 2 and 4
- **THEN** that row’s status cell shows light, dark, light, dark circles in channel order

### Requirement: Manual command terminal
Beneath the registered-device table the console SHALL provide Manual command. The button SHALL be enabled only when a registered row is selected. Activating it SHALL open a terminal dialog for that IEEE. The dialog SHALL include a read-only answers pane and, below it, a command input. Pressing Enter in the input SHALL send the typed body through the same host command path as that device’s MQTT `set` topic and SHALL NOT require a broker. When the selected device has more than one channel (`channels` `0` or `2`–`16`), the dialog SHALL include a channel dropdown; the chosen channel SHALL select the destination endpoint the same way MQTT suffix or `ch-<ep>##` mapping would. Escape SHALL dismiss the dialog without sending. New inbound messages for that IEEE SHALL append to the answers pane while the dialog is open.

#### Scenario: Send ON like MQTT set
- **WHEN** the operator selects a single-channel device, opens Manual command, types `ON`, and presses Enter
- **THEN** the host sends that body on the same path as a publish to that device’s stored command topic

#### Scenario: Channel dropdown
- **WHEN** the selected device has `channels` `4` and the operator chooses channel 3, types `OFF`, and presses Enter
- **THEN** the host sends `OFF` to endpoint 3 the same way MQTT `set/3` would

#### Scenario: Disabled without selection
- **WHEN** no registered row is selected
- **THEN** Manual command is disabled

### Requirement: Devices row type actions
When the pointer is over a registered-device row that has type actions, the console SHALL show a compact **…** control on the right of that row. Activating **…** SHALL open a menu of actions for that row’s stored type. When `channels` is `2` through `16`, or `channels` is `0` with more than one known endpoint, the top-level menu SHALL list actions only (ON/OFF/TOGGLE or OPEN/CLOSE/STOP). Each action SHALL show a submenu marker and open a submenu whose items are channel numbers only (`1`, `2`, `3`). The submenu SHALL stay inside the visible Devices card so it does not add a card scrollbar. When `channels` is `1`, or `channels` is `0` with no extra endpoints, the menu SHALL list actions once with no channel submenu. `onOff` actions SHALL be ON, OFF, and TOGGLE. `windowCovering` actions SHALL be OPEN, CLOSE, and STOP. `iasZone` and `unknown` SHALL have no type actions and SHALL NOT show **…**. Choosing an action SHALL send that body on the same path as Manual command / MQTT `set` for that IEEE and channel. Activating **…** or a menu item SHALL NOT open the device edit dialog. The table column order SHALL stay online, name, type, status, battery, RSSI; **…** is an overlay, not a new column.

#### Scenario: Hover shows ellipsis
- **WHEN** the pointer is over a registered `onOff` row
- **THEN** a **…** control appears on the right of that row

#### Scenario: Single-channel on/off
- **WHEN** the operator opens **…** on an `onOff` device with `channels` `1` and chooses ON
- **THEN** the host sends `ON` on the same path as Manual command for that IEEE without opening edit

#### Scenario: Multi-channel on/off
- **WHEN** the operator opens **…** on an `onOff` device with `channels` `3`
- **THEN** the top-level menu is ON, OFF, and TOGGLE, and each action’s submenu is `1`, `2`, and `3` only

#### Scenario: Covering stop on channel 2
- **WHEN** the operator chooses STOP for channel 2 on a `windowCovering` device with `channels` `2`
- **THEN** the host sends `STOP` to endpoint 2 the same way MQTT `{set}/2` would

#### Scenario: No actions for IAS
- **WHEN** the pointer is over a registered `iasZone` row
- **THEN** **…** is not shown

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

### Requirement: Maintenance theme picker
System → Maintenance SHALL show a Theme control with options **Light** and **Dark**. Light SHALL be the current gray/white chrome. Dark SHALL use a cool slate-night dashboard: near-black blue-gray page and sidebar, slightly lighter cards, cool off-white body text, muted blue-gray secondary text, and steel-blue primary buttons with dark labels. No magenta, pink, or red-violet accents. Changing the picker SHALL apply that theme to the whole console immediately without a reload. The last **saved** theme for the **signed-in user** SHALL load after login; the login form SHALL use Dark. Changing the picker without Save SHALL NOT persist; a later reload with the same session SHALL restore that user’s last saved theme.

#### Scenario: Dark applies immediately
- **WHEN** the operator opens Maintenance and selects Dark
- **THEN** the page, sidebar, cards, fields, dialogs, tables, log viewer, and buttons use the dark tokens without a reload

#### Scenario: Unsaved change is lost on reload
- **WHEN** the signed-in user’s saved theme is Light, the operator selects Dark, and then reloads without Save
- **THEN** the console opens in Light

### Requirement: Theme save icon
Immediately to the right of the Theme picker SHALL be a compact icon-only Save control (not a full-width bar). Activating it SHALL persist the picker value on the signed-in user without restart. After persist, a later login of that same user SHALL open in that theme.

#### Scenario: Save dark
- **WHEN** the signed-in operator selects Dark and clicks the Theme Save icon
- **THEN** a later login of that same user opens in Dark

#### Scenario: Icon sits beside picker
- **WHEN** the operator opens System → Maintenance
- **THEN** the Save icon is on the same row, immediately to the right of the Theme picker

### Requirement: Login form before the console
The first time a browser opens the console URL without a valid session, the operator MUST see a login form and MUST NOT see the sidebar console. The form MUST include user name, password, and a Remember me checkbox. The form MUST NOT be HTTP Basic. The login view MUST use the existing dark (slate-night) theme. Field layout beyond those three controls MAY be refined later. After a successful login, a simple user MUST see the separate control-console page (top tabs, no gateway sidebar). A user who has any operator role MUST see the gateway console. After logout or when the session ends, the login form MUST return.

#### Scenario: First visit
- **WHEN** a browser opens `http://<host-ip>/` with no session
- **THEN** the page shows the dark login form with user name, password, and Remember me, and does not show Status or other sidebar sections

#### Scenario: Successful login
- **WHEN** the operator submits a valid user name and password
- **THEN** the login form is hidden and the console sidebar is shown

#### Scenario: Simple user login opens control page
- **WHEN** a simple user submits a valid user name and password
- **THEN** the gateway sidebar is not shown and the control-console page is shown

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

### Requirement: Device controls follow roles
On Devices, a user without `addDevices` and without `isAdmin` MUST NOT see or complete Add device. A user without `editDevices` and without `isAdmin` MUST NOT open the parameter edit dialog for a registered device. A user without `removeDevices` and without `isAdmin` MUST NOT delete a registered device. A user without `controlDevices` and without `isAdmin` MUST NOT see row type-action controls (the **…** menu) or Manual command, and MUST NOT send those device commands from the Devices page. `isAdmin` MUST retain full Devices controls. Delete MUST still ask the operator to confirm. Control-console widget commands are out of scope for this requirement.

#### Scenario: Simple user actions only
- **WHEN** a simple user opens Devices
- **THEN** Add device is not available, edit parameters is not available, delete is not available, and the **…** menu and Manual command are not shown

#### Scenario: No controlDevices hides actions
- **WHEN** a signed-in user has Devices access but neither `controlDevices` nor `isAdmin`
- **THEN** the **…** menu and Manual command are not shown

#### Scenario: controlDevices can command
- **WHEN** a user has `controlDevices` and not `isAdmin`
- **THEN** that user can use the **…** menu and Manual command when the device type supports them

#### Scenario: addDevices without edit
- **WHEN** a user has `addDevices` and not `editDevices` or `isAdmin`
- **THEN** that user can add a device and cannot open edit on an already registered row

### Requirement: Security tab manages users
Security MUST show a table of users (user name, added-at, roles, blocked, theme) and MUST allow add and edit dialogs for operators who have `editUsers` or `isAdmin`. Password fields MUST never display a stored hash. The user dialog MUST include a Console editor checkbox (`editConsoles`), an **Other device control** checkbox (`controlDevices`), and a group of checkboxes for existing control consoles (`consoles`). An `editUsers` operator who is not `isAdmin` MUST NOT create `isAdmin`, MUST NOT edit or delete a user who has `isAdmin`, and MUST NOT see an enabled admin-role control for those actions. `isAdmin` MUST be able to create and edit `isAdmin` users. Deleting a user MUST ask the operator to confirm. A save, block, role change, or delete that would leave zero unlocked `isAdmin` users MUST fail and MUST show an error; the users table MUST stay unchanged.

#### Scenario: Non-admin cannot create admin
- **WHEN** an `editUsers` operator opens Add user
- **THEN** the admin role cannot be assigned

#### Scenario: Admin row is locked for editUsers
- **WHEN** an `editUsers` operator views a user who has `isAdmin`
- **THEN** that operator cannot save changes to that row or delete it

#### Scenario: Last unlocked admin cannot be blocked
- **WHEN** the only unlocked `isAdmin` user is edited to blocked
- **THEN** the console does not persist the change and shows an error

#### Scenario: Assign consoles in user dialog
- **WHEN** consoles `Kitchen` and `Hall` exist and the operator edits user `bob`
- **THEN** the dialog shows checkboxes for `Kitchen` and `Hall` and saving checked `Kitchen` stores that assignment

#### Scenario: Other device control checkbox
- **WHEN** an `editUsers` or `isAdmin` operator opens Add or Edit user
- **THEN** the dialog shows an Other device control checkbox bound to `controlDevices`

### Requirement: Appearance theme is the signed-in user
The Theme picker and Save icon MUST stay on System → Maintenance → Appearance (same place as today). After login the console MUST apply that user’s saved theme. Saving Theme there MUST persist that theme on the signed-in user only. The login form MUST stay dark regardless of any user theme. A later login of the same user MUST reopen in that user’s last saved theme. The control MUST NOT move to Security or to a new page.

#### Scenario: User theme after login
- **WHEN** user `alice` has theme `light` and signs in from the dark login form
- **THEN** the console chrome switches to Light

#### Scenario: Save theme is per user
- **WHEN** `alice` saves Dark and `bob` has Light saved
- **THEN** `alice` later logs in to Dark and `bob` later logs in to Light

### Requirement: Search dialog offers WIFI DEVICE

The Add-device search dialog SHALL include an **MQTT DEVICE** button. Activating it SHALL close or leave the search dialog and open the device parameter dialog for a new `mqtt` device with transport locked to MQTT. Pairing search SHALL NOT be required. IEEE SHALL be editable on that create form.

#### Scenario: Open WiFi create

- **WHEN** the operator opens Add device and clicks MQTT DEVICE
- **THEN** the parameter dialog opens with transport MQTT (not editable) and an editable IEEE field

### Requirement: Parameter dialog shows transport before NAME

The device parameter dialog SHALL show a transport field (Zigbee or MQTT) immediately before the NAME field. Transport SHALL be readonly after it is set for the dialog session. Zigbee add/edit SHALL show Zigbee. MQTT DEVICE create and `mqtt` edit SHALL show MQTT.

#### Scenario: Zigbee edit order

- **WHEN** the operator opens parameters for a registered `zigbee` device
- **THEN** the settings fields include transport Zigbee immediately before NAME

#### Scenario: WiFi create shows WiFi

- **WHEN** the operator opens parameters via MQTT DEVICE
- **THEN** transport shows MQTT and cannot be changed to Zigbee in that dialog

### Requirement: MQTT card edits server-born topic and interval
The MQTT section SHALL show and allow saving SERVER BORN TOPIC and BORN INTERVAL (minutes). GET `/api/mqtt` and POST `/api/mqtt` SHALL include `serverBornTopic` and `bornIntervalMin`. The form SHALL load stored values when the MQTT section opens. Interval input SHALL use the stored clamp rules (5–120, default 20). Settings export and restore SHALL include both fields.

#### Scenario: Load born fields
- **WHEN** the host has `serverBornTopic` `homeassistant/status` and `bornIntervalMin` 30 and the operator opens MQTT
- **THEN** those fields show that topic and 30

#### Scenario: Save born fields
- **WHEN** the operator sets SERVER BORN TOPIC to `z2m/birth`, BORN INTERVAL to 15, and saves MQTT
- **THEN** GET `/api/mqtt` returns those values

### Requirement: Pointer-down outside an overlay dismisses it

The console SHALL dismiss an overlay dialog only when the pointer-down target is that overlay (the dimmed area outside the dialog panel). Pointer-up on the overlay MUST NOT dismiss when the pointer-down started on the dialog panel (including a text selection that ends outside the panel). Pointer-down on the panel MUST NOT dismiss. Dismiss SHALL use the same path as that overlay’s Cancel or Close (no save, no delete confirm; search dismiss still stops pairing). When more than one overlay is visible, only the front-most SHALL dismiss.

#### Scenario: Drag-select then pointer-up on overlay

- **WHEN** a dialog is open, the operator pointer-downs on text inside the panel, drags, and pointer-ups on the dimmed overlay
- **THEN** the dialog stays open and the selection is not treated as a dismiss

#### Scenario: Pointer-down on overlay dismisses

- **WHEN** a dialog is open and the operator pointer-downs on the dimmed overlay outside the panel
- **THEN** the dialog dismisses the same way as Cancel or Close for that overlay

### Requirement: Readonly fields look distinct from editable fields

Readonly and disabled form controls in console field rows SHALL use a slightly grayed background and muted text relative to editable controls of the same type so the operator can tell they cannot edit those values.

#### Scenario: IEEE readonly on Zigbee edit

- **WHEN** the operator opens the parameter dialog for a registered Zigbee device
- **THEN** the IEEE field is not editable and its background is visibly grayer than the NAME field

### Requirement: Session idle follows operator activity

While the operator is signed in, the console SHALL treat pointer, keyboard, and form input on the signed-in UI as operator activity and SHALL reset the session idle timer on that activity. Automatic requests (including session-alive polls and status or log polls) MUST NOT reset the idle timer. After the idle window with no operator activity, the console MUST return to the login form.

#### Scenario: Working in the console keeps the session

- **WHEN** an `isAdmin` operator stays on the console and uses it (clicks or types) throughout a period longer than 10 minutes since login
- **THEN** the session stays valid and the login form does not appear

#### Scenario: Idle without using the console logs out

- **WHEN** an `isAdmin` operator leaves the signed-in console unused for 10 minutes
- **THEN** the next check shows the login form and protected APIs reject the old session

### Requirement: Consoles section lists and edits layouts
Consoles MUST show a table with columns Console name, Active, and Users (user names assigned that console). Add and row edit MUST open a large dialog for name, active, widgets, export, import (empty only), and Save. Closing the overlay MUST follow the same dirty-check and mousedown-dismiss rules as other dialogs. Delete MUST ask for confirmation.

#### Scenario: Table columns
- **WHEN** console `Kitchen` is active and assigned to `alice` and `bob`
- **THEN** the row shows `Kitchen`, active, and both user names

#### Scenario: Large edit dialog
- **WHEN** the operator adds or edits a console
- **THEN** a large dialog opens for that console’s content

### Requirement: Hardware tab sets blue and green LED brightness

System → Hardware SHALL show SPI speed plus **BLUE LED brightness** and **GREEN LED brightness** as integer fields from 1 to 100 (percent). GET `/api/hardware` SHALL return `spiSpeedHz`, `blueLedBrightness`, and `greenLedBrightness`. POST `/api/hardware` SHALL require all three, reject values outside 1–100 for either brightness, persist them with hardware settings, apply them without requiring a reboot, and push brightness to the slave when the link is up. Factory default for both brightness fields SHALL be 100. Only `isAdmin` SHALL use Hardware.

#### Scenario: Load shows brightness

- **WHEN** an admin opens System → Hardware
- **THEN** blue and green brightness show the stored values (default 100 if never saved)

#### Scenario: Save applies brightness

- **WHEN** the admin sets blue to 40 and green to 70 and clicks Save
- **THEN** GET `/api/hardware` returns those values and host blue/green LEDs use them while on

#### Scenario: Out of range rejected

- **WHEN** POST `/api/hardware` sends `blueLedBrightness` 0 or 101
- **THEN** the host responds 400 and does not change stored brightness

### Requirement: Login Enter focuses empty password

On the login form, when the operator presses Enter and the password field is empty, the console MUST move focus to the password field and MUST NOT submit the login request. When the password is non-empty, Enter MAY submit as today.

#### Scenario: Empty password Enter

- **WHEN** the login form is shown, the password is empty, and the operator presses Enter
- **THEN** focus moves to the password field and no login request is sent

#### Scenario: Password filled Enter

- **WHEN** the login form has a non-empty password and the operator presses Enter
- **THEN** the form submits as today

### Requirement: Error and success plates hide after ten seconds

Whenever the web console shows an error plate (`save-error`) or success plate (`save-success`), including login errors, that plate MUST become hidden automatically after 10 seconds unless a newer message replaces it sooner. Hiding MUST NOT clear unrelated form fields. Plates on the Update card that report the reboot-wait request result are exempt and MUST follow the firmware Update result-plate requirement instead.

#### Scenario: Success auto-hides

- **WHEN** a settings Save shows a success plate
- **THEN** that plate is hidden about 10 seconds later without a page reload

#### Scenario: Error auto-hides

- **WHEN** a Save shows an error plate
- **THEN** that plate is hidden about 10 seconds later

#### Scenario: Newer message resets the timer

- **WHEN** an error plate is visible and a later Save shows a success plate before 10 seconds
- **THEN** the success plate is shown and the previous error is not left visible after the new plate’s own 10-second window

#### Scenario: Firmware Update dialog plates exempt

- **WHEN** the Update card shows its reboot-wait success or error plate
- **THEN** that plate is not auto-hidden after ten seconds
### Requirement: Firmware Update dialog result plates stay visible

Success and error plates for the firmware reboot-wait result on the Update card (`firmwareRebootSuccess` and `firmwareRebootError`) MUST remain visible until the operator clears the update progress UI or starts a new firmware update. Those plates MUST NOT be hidden by the global ten-second auto-hide used for other `save-error` / `save-success` plates.

#### Scenario: Reboot success plate stays

- **WHEN** the Update card shows the reboot-wait success plate after the router comes back
- **THEN** that plate stays visible past ten seconds until the operator clears the update progress UI

#### Scenario: Reboot error plate stays

- **WHEN** the Update card shows the reboot-wait error plate after the wait times out
- **THEN** that plate stays visible past ten seconds until the operator clears the update progress UI

#### Scenario: Other plates still auto-hide

- **WHEN** a settings Save shows a success or error plate outside the firmware Update progress UI
- **THEN** that plate still hides after about ten seconds as today

### Requirement: HTTP POST bodies are bounded and request-isolated
The host web console MUST collect each authenticated JSON POST body with a fixed maximum size. Bodies larger than that limit MUST be rejected with an HTTP error and MUST NOT be applied. Concurrent or overlapping body uploads MUST NOT corrupt another request’s body. Chunk append MUST NOT grow storage one character at a time.

#### Scenario: Oversized restore rejected
- **WHEN** an admin posts a body larger than the configured maximum to a JSON settings or restore endpoint
- **THEN** the host responds with an error and does not change persisted settings or the device list from that body

#### Scenario: Concurrent posts do not share one body buffer
- **WHEN** two HTTP POST handlers receive body chunks overlapping in time
- **THEN** each request uses only its own collected body when the handler runs

### Requirement: Large console JSON does not grow unbounded Strings
Device-list, settings-export, and gateway-status JSON responses served by the host MUST be produced without unbounded Arduino `String` concatenation that reallocates for every field. The operator-visible JSON field set for those APIs MUST remain compatible with the existing console.

#### Scenario: Devices list still loads
- **WHEN** a signed-in operator opens Devices after this change
- **THEN** `GET` of the registered-device list returns JSON the console can render as before

#### Scenario: Settings export still downloads
- **WHEN** an admin exports settings from the console
- **THEN** the download contains the same logical groups and device list the export used before

### Requirement: Console stays usable during STA scan and MQTT reconnect
While the host performs a blocking Wi‑Fi network scan, an MQTT TCP connect/reconnect, or the post-connect remote MQTT session subscription attach for the device map, the HTTP console on port 80 and host SPI keepalive work MUST remain able to progress within a few seconds (no multi-tens-of-seconds freeze of the main service loop caused by those operations running as one uninterrupted burst on that loop). Those operations MUST NOT reboot the host via stack overflow.

#### Scenario: Status answers during MQTT reconnect
- **WHEN** the remote MQTT broker is unreachable and the host is attempting reconnect
- **THEN** `GET /api/status` still returns within a few seconds

#### Scenario: Status answers during subscription attach

- **WHEN** remote MQTT has just connected and the host is still installing device topic subscriptions for a large device map
- **THEN** `GET /api/status` still returns within a few seconds and the host does not reboot from a stack-canary panic

#### Scenario: Console answers during STA rescan
- **WHEN** STA reconnect triggers a network scan
- **THEN** the web console on the current AP or STA address still answers HTTP within a few seconds

### Requirement: Update tab can pause host slave resets for 120 seconds

System → Update SHALL show an admin-only **Wait for update slave** control for a USB slave-programming window. It SHALL start a 120 second host slave-reset pause, show live remaining time on the control, cancel on a second press, and load real remaining time when the Update tab becomes active.

#### Scenario: Start pause

- **WHEN** an admin opens System → Update and activates Wait for update slave while no pause is active
- **THEN** the host starts a 120 second slave-reset pause (so the operator can flash the slave over USB without host EN pulses) and the button shows a countdown that decreases toward zero

#### Scenario: Show remaining on tab enter

- **WHEN** a pause is already active and the admin opens or re-selects the Update tab
- **THEN** the Wait for update slave control shows the remaining seconds until the pause ends (matching host state)

#### Scenario: Cancel pause

- **WHEN** a pause is active and the admin activates Wait for update slave again
- **THEN** the host ends the pause immediately and the control returns to the inactive label without a countdown

#### Scenario: Pause expires

- **WHEN** a pause reaches zero remaining time without cancel
- **THEN** the host ends the pause and the control returns to the inactive label

### Requirement: Status Memory shows free RAM and PSRAM
The Status Memory card MUST be titled to reflect free memory (for example **Free memory** or group title **Memory** with row labels **Free RAM** / **Free PSRAM**). Values MUST show free heap/PSRAM in kB and free percent of total (not used amounts). When PSRAM is absent, PSRAM MUST show as unavailable (em dash or equivalent), not zero used.

#### Scenario: Free heap displayed
- **WHEN** the operator opens Status and the host reports free heap
- **THEN** the Memory card shows free RAM in kB and free percent, not used heap

#### Scenario: Free PSRAM when present
- **WHEN** PSRAM is present
- **THEN** the Memory card shows free PSRAM in kB and free percent

### Requirement: Console editor can bind automations
In the Consoles edit dialog, adding a binding MUST allow choosing a registered device or an automation. Automation bindings MUST appear as On/Off rows in the editor preview (display only; no active change until Save + user view). Saved layouts MUST round-trip automation bindings through export/import and settings restore.

#### Scenario: Add automation binding
- **WHEN** the editor adds automation `yard-light` to a widget and saves
- **THEN** the console JSON includes that automation binding and the user page shows its On/Off row

### Requirement: Place view-mode consoles match adaptive user layout
When the gateway console shows an assigned console in Place (or equivalent operator view mode), that view-mode surface MUST use the same full-viewport-friendly shell sizing and the same one- vs two-column widget width rules as the simple-user control page, including `fullWidth` spanning. The Consoles editor dialog MUST author widgets in a vertical stack only (not a multi-column authoring grid). Saved console `columns` MUST NOT drive editor placement.

#### Scenario: Place narrow width stacks widgets
- **WHEN** an operator opens Place view for a console whose content width is under 13 cm
- **THEN** visible widgets stack in one full-width column as on the simple-user page

#### Scenario: Editor keeps authoring columns
- **WHEN** an editor opens the Consoles edit dialog for a console saved with three columns
- **THEN** the edit surface shows a single vertical widget stack and does not use three-column placement or left/right targets

### Requirement: Hardware tab keeps Save and Reset visible
System → Hardware MUST scroll its settings fields when content exceeds the panel height and MUST keep the Save and Reset controls visible at the bottom of the Hardware panel (not scrolled out of view with the fields).

#### Scenario: Short viewport Hardware
- **WHEN** an admin opens System → Hardware on a viewport shorter than the Hardware fields
- **THEN** the fields scroll inside the panel and Save and Reset remain visible at the bottom

### Requirement: Automation editor uses spaced groups including General
The automation edit dialog body MUST present groups in this order with about one blank line of vertical space between them: **General**, **Trigger**, **Condition**, **Actions**. General MUST be a single container holding Active, Name, Group, then optional Min trigger interval if still shown, then Debug—without wrapping each of those controls in its own field card. The Trigger group MUST be a single container that starts with the countdown timer controls (enable labeled **Use countdown timer**), then **Triggered by devices** (heading, trigger-device rows, and **Add device to trigger**). The dialog MUST NOT show a Title field.

#### Scenario: General order without Title
- **WHEN** the operator opens the automation edit dialog
- **THEN** General shows Active, then Name, then Group, and no Title control appears

#### Scenario: Group spacing
- **WHEN** the operator views the edit dialog with Trigger and Condition sections present
- **THEN** General, Trigger, Condition, and Actions are separated by visible vertical spacing

#### Scenario: Timer at start of Trigger
- **WHEN** the operator views the Trigger group
- **THEN** Use countdown timer appears before Triggered by devices inside the same container

### Requirement: Automation condition group has an enable checkbox
The Condition group MUST include a checkbox that enables the main condition editor. When the checkbox is unchecked, the main condition editor MUST be hidden. That checkbox state MUST persist with the automation and round-trip through dialog Save, export/import, and settings restore.

#### Scenario: Hide condition when disabled
- **WHEN** the operator unchecks the Condition enable control
- **THEN** the condition tree editor is hidden

#### Scenario: Condition enable survives save
- **WHEN** the operator disables Condition, saves, and reopens the automation
- **THEN** Condition remains disabled and the editor stays hidden

### Requirement: Automation General includes Debug
General MUST end with a Debug checkbox. That flag MUST persist with the automation and round-trip through Save, export/import, and settings restore. Runtime debug behavior is defined by the automations capability.

#### Scenario: Debug at end of General
- **WHEN** the operator opens the automation edit dialog
- **THEN** Debug is the last control in the General group

### Requirement: Console editor sets widget viewport
In the Consoles edit dialog, each widget MUST expose a Viewport control with options Vertical, Grid, and Composite (default Vertical). Horizontal MUST NOT be offered. When Grid is selected, the dialog MUST show Columns and Rows integer fields defaulting to 2 and 2. When Composite is selected, the dialog MUST show composite row/cell authoring (Add row, per-row height configurer on the left, per-cell width configurers stacked horizontally above cells). Changing viewport or layout size MUST update the in-dialog preview without persisting until Save. Cancel or reload without Save MUST discard unsaved viewport edits. Vertical and Grid preview and binding behaviors MUST match today’s Vertical and Grid.

#### Scenario: Grid fields appear
- **WHEN** the editor sets a widget Viewport to Grid
- **THEN** Columns and Rows fields are visible and default to 2 and 2 for a new grid selection without prior values

#### Scenario: Vertical hides grid fields
- **WHEN** the editor sets Viewport to Vertical
- **THEN** Columns and Rows fields are hidden and composite row editors are hidden

#### Scenario: Composite editor appears
- **WHEN** the editor sets Viewport to Composite
- **THEN** Add row and row/cell size configurers are visible and Columns/Rows are hidden

#### Scenario: No Horizontal option
- **WHEN** the editor opens the Viewport select
- **THEN** Horizontal is not listed

### Requirement: Console device dialog matches automation trigger fields
When adding or editing a device binding (not automation), the binding dialog MUST offer device, channel, and param selects with the same dependent refresh behavior as automation “Add device to trigger”, including custom cluster and attrId when param is `custom`. Existing title, unit, and decimals fields MUST remain where they apply. Automation target mode MUST keep the automation picker and MUST NOT show gauge or custom cluster fields.

#### Scenario: Param list follows device
- **WHEN** the editor picks a registered device in the binding dialog
- **THEN** the param list refreshes for that device the same way as automation listen

#### Scenario: Custom shows cluster and attr
- **WHEN** the editor sets param to `custom`
- **THEN** cluster and attrId fields become visible and are included in the saved binding body

### Requirement: Console device dialog gauge config
When the selected binding parameter is numeric (including Measurement-class `sensorValue`, battery, position, light level, or custom) and the device is not a binary sensor type, the binding dialog MUST show a View style control with Default and Gauge. When Gauge is selected, the dialog MUST show a Gauge group: type Arc (default) or Linear, Min value, Max value, and a list of color ranges. Each list row shows top limit and color; the dialog MUST provide Top and color-picker fields below the list to append a range. Bottom of each range is derived (first from Min, later from previous top). View style Gauge MUST be hidden or ignored for On/Off status, covering move status, automation bindings, and binary sensor types.

#### Scenario: Temperature offers Gauge
- **WHEN** an editor opens a device binding for a temperature Measurement device and the param is Sensor value
- **THEN** the dialog shows View style with Default and Gauge

#### Scenario: On/Off hides Gauge
- **WHEN** an editor selects an On/Off device with param Status
- **THEN** View style Gauge is hidden

#### Scenario: Gauge group appears
- **WHEN** the editor chooses View style Gauge for a temperature binding
- **THEN** type, min, max, and the color-range list editor are visible

#### Scenario: Add color range
- **WHEN** the editor enters a Top and color and adds a range
- **THEN** that range appears in the list and is included when the binding is saved

### Requirement: Console device dialog binary icon View style
When the selected target is a binary sensor type (Binary Input / Output / Value or BACnet binary), an On/Off device, or an automation, the binding dialog MUST show View style with Default and Icon (not Gauge). When Icon is selected, the dialog MUST show an icon picker of Material Design Icons (Home Assistant / Pictogrammers), including at least door, window, motion, water, pump, water-pump, heat-pump, and engine icons. Measurement numeric bindings MUST keep Default/Gauge and MUST NOT use this Icon View style path.

#### Scenario: Binary offers Icon
- **WHEN** an editor opens a binding for a binaryInput device
- **THEN** View style shows Default and Icon and does not show Gauge

#### Scenario: On/Off offers Icon
- **WHEN** an editor opens a binding for an On/Off device
- **THEN** View style shows Default and Icon

#### Scenario: Automation offers Icon
- **WHEN** an editor sets the binding target to automation
- **THEN** View style shows Default and Icon and the icon picker is available when Icon is selected

#### Scenario: Pick pump icon
- **WHEN** the editor selects View style Icon and chooses `mdi:pump`
- **THEN** the saved binding stores `viewStyle` `icon` and `icon` `mdi:pump`

### Requirement: Measurement binding param defaults to Sensor value
When adding or refreshing a console device binding for a Measurement-class device (temperature, pressure, humidity, illuminance, flow, metering, electrical measurement, and the same set the console already treats as measurement for display), the param picker MUST offer Sensor value as the primary reading and MUST NOT leave an unmatched Status option selected merely because the dialog defaulted to `status`. If the previous param is not in that device’s catalog, the dialog MUST select the first catalog parameter instead of injecting the unmatched value as selected.

#### Scenario: New temperature binding selects Sensor value
- **WHEN** an editor adds a device binding and chooses a temperature Measurement device
- **THEN** the param select shows Sensor value selected and View style Gauge is available

#### Scenario: Switching from On/Off to pressure drops orphan Status
- **WHEN** an editor changes the binding device from an On/Off device (param Status) to a pressure Measurement device
- **THEN** the param select does not keep Status selected and instead selects Sensor value (or another catalog param for that device)

### Requirement: User and Place pages render viewport and gauges
The simple-user control page and Place view-mode MUST lay out each widget’s bindings according to that widget’s viewport (`vertical`, `grid`, or `composite`) and MUST render gauge-styled numeric bindings as scaled arc or linear gauges and icon-styled binary, On/Off, and automation bindings as Material Design Icons. Edit-mode preview in the Consoles dialog SHOULD reflect the same viewport, gauge, and icon choices using display-only controls (no device commands). Binding view styles MUST keep existing Default / Gauge / Icon behavior.

#### Scenario: User page grid gauges
- **WHEN** a simple user opens a console whose widget uses grid viewport and arc gauges
- **THEN** bindings appear in the grid and gauges fit their cells

#### Scenario: Place matches user viewport
- **WHEN** an operator opens the same console in Place view mode
- **THEN** viewport and gauge presentation match the simple-user page for that layout

#### Scenario: User page binary icon
- **WHEN** a simple user opens a console with a binaryInput binding using view style icon
- **THEN** the binding shows the chosen Material Design Icon instead of a default text/control-only plate

#### Scenario: User page On/Off icon
- **WHEN** a simple user opens a console with an On/Off binding using view style icon
- **THEN** the binding shows the icon control (not a switch)

#### Scenario: User page automation icon
- **WHEN** a simple user opens a console with an automation binding using view style icon
- **THEN** the binding shows the icon control for that automation

#### Scenario: User page composite layout
- **WHEN** a simple user opens a console whose widget uses composite viewport with mixed cm and unit cells
- **THEN** cells size per the composite size rules and device cells show their bindings

### Requirement: Binding dialog icon scale
When View style is Icon, the Consoles binding dialog MUST show an Icon scale numeric field (default `1`). Saving MUST store `iconScale` on the binding. The field MUST be hidden when View style is not Icon.

#### Scenario: Scale field appears for icon
- **WHEN** an editor selects View style Icon for an On/Off, binary, or automation binding
- **THEN** the dialog shows Icon scale defaulting to `1`

#### Scenario: Save icon scale
- **WHEN** the editor sets Icon scale to `1.25` and saves
- **THEN** the binding stores `iconScale` `1.25`

### Requirement: Binding dialog show title and title position
The Consoles binding dialog MUST show a Show title checkbox for every device and automation binding (default checked). When Show title is checked, the dialog MUST show a Title position select with Left, Right, Top, and Bottom (default Left). Saving MUST store `showTitle` and `titlePosition`. When Show title is unchecked, Title position MAY stay hidden and the saved `titlePosition` MAY keep its last value or default `left`.

#### Scenario: Show title for automation
- **WHEN** an editor opens an automation binding
- **THEN** Show title is available and checked by default for a new binding

#### Scenario: Title position top
- **WHEN** the editor checks Show title, chooses Top, and saves
- **THEN** the binding stores `showTitle` true and `titlePosition` `top`

### Requirement: Place and user honor icon scale and title layout
The simple-user control page and Place view-mode MUST apply each binding’s `iconScale` as a multiplier on top of viewport fit for icon controls, MUST hide the title when `showTitle` is false, and MUST place the title per `titlePosition` when shown. Edit-mode preview SHOULD reflect the same layout without sending device commands.

#### Scenario: User page hides title
- **WHEN** a simple user opens a console whose binding has `showTitle` false
- **THEN** the binding row shows no title text

#### Scenario: User page title on the right
- **WHEN** a simple user opens a console whose binding has `titlePosition` `right`
- **THEN** the title appears to the right of the control or icon

#### Scenario: Icon larger than cell fit alone
- **WHEN** a simple user opens a grid widget with an icon binding whose `iconScale` is greater than `1`
- **THEN** the icon is larger than the same binding would be at `iconScale` `1` in that cell

### Requirement: Binding dialog content align for all view styles
The Consoles binding dialog MUST show Vertical align for every device and automation binding (default Center) in every View style. Horizontal align MUST appear when Show title is unchecked and MUST be hidden when Show title is checked so titled rows keep the classic name-left / control-right layout. Saving MUST store `alignH` and `alignV`.

#### Scenario: Horizontal align hidden when title shown
- **WHEN** an editor opens a binding with Show title checked
- **THEN** Horizontal align is hidden and Vertical align remains available

#### Scenario: Align visible when title hidden
- **WHEN** an editor unchecks Show title on a Default-view binding
- **THEN** Horizontal align and Vertical align are visible

#### Scenario: Save default-view align
- **WHEN** the editor sets Horizontal align to Left and Vertical align to Top on a Default-view binding with Show title unchecked and saves
- **THEN** the binding stores `alignH` `left` and `alignV` `top`

### Requirement: Place and user honor content align
The simple-user control page and Place view-mode MUST apply each binding’s `alignV` for Default, Icon, and Gauge view styles. When `showTitle` is false, they MUST also apply `alignH`. When `showTitle` is true with title position left or right, the row MUST keep name and control on one line (control opposite the title). Edit-mode preview SHOULD reflect the same layout without sending device commands. When Show title is unchecked in edit mode, the plate MUST show a muted registered device name (not the binding Title and not the IEEE).

#### Scenario: User page default view right align without title
- **WHEN** a simple user opens a console whose Default-view binding has `showTitle` false and `alignH` `right`
- **THEN** the binding content is right-aligned in its cell or slot

#### Scenario: Titled row keeps control on the same line
- **WHEN** a simple user opens a console whose binding has `showTitle` true and `titlePosition` `left`
- **THEN** the title and switch or icon appear on one row

#### Scenario: Edit preview shows muted device name
- **WHEN** an editor views a binding with `showTitle` false in the Consoles edit dialog
- **THEN** the plate shows the registered device name in muted text

### Requirement: Widget header to content gap
Console widgets on Place and the simple-user page MUST leave a **5mm** gap between the widget header (title / group On/Off) and the binding content list.

#### Scenario: Five millimetre header gap
- **WHEN** a simple user opens a console widget that has a title and at least one binding
- **THEN** the space between the widget title row and the first binding content is 5mm

### Requirement: Place and user gauges show unitless value text
On the simple-user control page and Place view-mode, gauge-styled bindings MUST show the numeric value without a measure unit suffix. Arc gauges MUST use the reduced center value font size from the control-consoles arc font requirement.

#### Scenario: User page temperature gauge text
- **WHEN** a simple user opens a console with a temperature arc gauge
- **THEN** the center value shows digits only (no °C) at the reduced arc font size
### Requirement: Update card hosts firmware progress inline

System → Update SHALL show firmware upload and joined-update progress on the Update card itself. The card SHALL include the three phase labels and percent bars (upload to master, RF firmware update, write on master), live status text, the post-host-write reboot wait (30 second countdown and background ping), and a way to clear the progress UI when the update is closeable. The console MUST NOT require a separate modal dialog overlay for these controls.

#### Scenario: Progress on the card

- **WHEN** the operator starts a firmware update from System → Update
- **THEN** phase percents and status text appear on the Update card without opening a modal overlay

#### Scenario: Reboot wait on the card

- **WHEN** the host is about to restart after a successful firmware write
- **THEN** the Update card shows the reboot warning, countdown, and ping result on the card

### Requirement: Update card disables start when update already active

When the operator opens System → Update, the console SHALL query the current firmware update status. If an update is already active (any non-idle in-progress phase), controls that start a new firmware update MUST be disabled until the active update finishes or fails. Opening the card while idle MUST leave those controls enabled.

#### Scenario: Active update on enter

- **WHEN** the operator opens System → Update while a firmware update is already active
- **THEN** the Update button (and equivalent start controls) are disabled

#### Scenario: Idle on enter

- **WHEN** the operator opens System → Update while no update is active
- **THEN** the Update button remains enabled for a new joined package

### Requirement: Maintenance reboot with two approvals

System → Maintenance SHALL show a **REBOOT** control for `isAdmin`. Activating it SHALL require two sequential confirmations before the host reboots. Canceling either confirmation MUST NOT reboot. After both approvals, the host SHALL reboot.

#### Scenario: Double confirm then reboot

- **WHEN** an admin clicks REBOOT and confirms twice
- **THEN** the router reboots

#### Scenario: First confirm canceled

- **WHEN** an admin clicks REBOOT and cancels the first confirmation
- **THEN** the router does not reboot and no second confirmation is shown

#### Scenario: Second confirm canceled

- **WHEN** an admin confirms the first reboot prompt and cancels the second
- **THEN** the router does not reboot

### Requirement: Login focuses username by default

When the login form is shown (initial load without a session, after logout, or after session end), the browser focus MUST move to the user name field when that field is empty, otherwise to the password field.

#### Scenario: Open login with empty username

- **WHEN** a browser opens the console URL with no session and the login form appears with an empty user name field
- **THEN** the user name field has keyboard focus

#### Scenario: Open login with remembered username

- **WHEN** the login form appears and the user name field is already filled
- **THEN** the password field has keyboard focus

### Requirement: MQTT save applies without router restart

`POST /api/mqtt` MUST persist MQTT settings and apply them on the running host without restarting the router. The success response MUST NOT claim that the device will restart. Wi-Fi save MAY still restart.

#### Scenario: Save MQTT remote settings

- **WHEN** an admin saves valid MQTT settings
- **THEN** the settings are stored, MQTT client/broker behavior follows the new settings without a router restart, and the response does not announce a restart

### Requirement: Device edit dialog groups MQTT fields
The device parameters (edit) dialog SHALL place MQTT topic fields inside a labeled **MQTT** container/group. For a device whose transport is not `mqtt`, the dialog SHALL show a **Publish to MQTT** checkbox before those topic fields. When that checkbox is unchecked, the MQTT topic fields SHALL be hidden. When checked, the topic fields SHALL be visible for editing. For `mqtt` transport devices, the checkbox SHALL NOT appear; topic fields remain available as today inside the MQTT group.

#### Scenario: Zigbee unchecked hides topics
- **WHEN** the operator edits a `zigbee` device and Publish to MQTT is unchecked
- **THEN** state, command, and availability topic fields are not shown

#### Scenario: Zigbee checked shows topics
- **WHEN** the operator edits a `zigbee` device and checks Publish to MQTT
- **THEN** the MQTT topic fields are shown inside the MQTT group below the checkbox

#### Scenario: MQTT device has no checkbox
- **WHEN** the operator edits an `mqtt` transport device
- **THEN** Publish to MQTT is not shown and topic fields remain editable in the MQTT group

#### Scenario: Save persists checkbox
- **WHEN** the operator unchecks Publish to MQTT on a zigbee device and Saves
- **THEN** after reload the checkbox is unchecked and topic fields stay hidden until checked again

### Requirement: Console editor vertical-only widget reorder
In the Consoles edit dialog, widgets MUST stack vertically. Move controls MUST offer only up and down. Empty spacer widget cards MUST NOT be shown or created when reordering. Drag-and-drop MUST reorder within the stack without inserting spacer placeholders.

#### Scenario: Move pad has up and down only
- **WHEN** the editor shows a widget move pad
- **THEN** left and right move buttons are absent and up and down remain

#### Scenario: Reorder without spacers
- **WHEN** the editor moves the second widget above the first
- **THEN** the draft contains exactly those two widgets in the new order with no spacer entries

### Requirement: Console editor Full width checkbox
Each real widget in the Consoles edit dialog MUST show a Full width checkbox bound to that widget’s `fullWidth` flag (unchecked by default). Changing it MUST update the draft and preview width behavior without persisting until Save.

#### Scenario: Checkbox defaults unchecked
- **WHEN** the editor adds a new widget
- **THEN** Full width is unchecked

#### Scenario: Check full width in draft
- **WHEN** the editor checks Full width on a widget and saves
- **THEN** the persisted widget has `fullWidth` true

### Requirement: Console editor composite cell authoring
When Viewport is Composite, the editor MUST provide Add row. Each row MUST show a height configurer on the left (value plus cm or units). Each cell MUST show a width configurer above it (value plus cm or units), stacked horizontally across the row. The editor MUST let the operator set cell content to text, device (binding dialog), or empty, and MUST offer an optional cell contour control. Preview MUST reflect sizes and content without sending device commands.

#### Scenario: Add row
- **WHEN** the editor clicks Add row on a composite widget
- **THEN** a new row appears with at least one cell and left height configurer

#### Scenario: Configure cell width units
- **WHEN** the editor sets a cell width to 2 units
- **THEN** the draft stores width 2 with unit `u` and the preview updates

#### Scenario: Empty cell with contour
- **WHEN** the editor sets a cell to empty and enables contour
- **THEN** preview shows an outlined empty cell and save persists those settings

### Requirement: Console recovers live links when the page becomes active

While the operator is signed in, when the console page becomes visible or shown again after being hidden or suspended, the console MUST ensure every live WebSocket that is supposed to be running is open or connecting, and MUST reopen any that are closed or otherwise not usable. After recovery, the signed-in UI MUST resume receiving router live events without requiring a full page reload.

#### Scenario: Devices live socket after tab returns

- **WHEN** the operator is signed in with devices live updates wanted, the tab was hidden long enough that the devices WebSocket closed, and the tab becomes visible again
- **THEN** the console reopens the devices live WebSocket and the Devices UI receives subsequent live updates from the router

#### Scenario: Consoles live socket after tab returns

- **WHEN** the operator is signed in with consoles live updates wanted, the consoles WebSocket is not open, and the page becomes active again
- **THEN** the console reopens the consoles live WebSocket and consoles/users/place live reloads resume

#### Scenario: Already healthy sockets stay up

- **WHEN** the page becomes active and every wanted live WebSocket is already open or connecting
- **THEN** the console does not tear those sockets down solely because the page became active

### Requirement: Console restarts wanted timers when the page becomes active

While the operator is signed in, when the console page becomes visible or shown again after being hidden or suspended, the console MUST ensure wanted interval timers (session watch and active section polls such as status, log, automation list, and pairing/found polls when those flows are active) are running again if they had stopped, and MUST trigger an immediate refresh for those active live views so the UI catches up after missed ticks.

#### Scenario: Status poll resumes after background

- **WHEN** the operator is signed in with Status polling wanted, the tab was backgrounded, and the page becomes visible again
- **THEN** Status polling is running and the Status panel refreshes from the router without a full page reload

#### Scenario: Session watch remains after activation

- **WHEN** the operator is signed in, session watch was wanted, and the page becomes active again
- **THEN** the session-alive watch timer is running

#### Scenario: Login page does not start live recovery

- **WHEN** the login form is shown (no signed-in session) and the page becomes visible again
- **THEN** the console does not open live WebSockets or start signed-in poll timers

### Requirement: User-data parameter dialog edits field schema

For type `userData`, the parameter dialog MUST show a User data fields table (columns: nullable checkbox, name, type) with per-row Remove, Up, and Down. Add is draft-only until Save; Cancel discards. Types are `int`, `float`, `bool`, `string`. Hide the table for other types.

#### Scenario: Reorder then save

- **WHEN** the operator edits a `userData` device, moves the second field above the first, and Saves
- **THEN** after reload the schema list order matches the reordered table

#### Scenario: Cancel discards draft rows

- **WHEN** the operator Adds a field row in the dialog and Cancels without Save
- **THEN** after reopen the stored schema does not include that unsaved row

#### Scenario: Hidden for On/Off

- **WHEN** the operator opens parameters for an `onOff` device
- **THEN** the User data fields table is not shown
