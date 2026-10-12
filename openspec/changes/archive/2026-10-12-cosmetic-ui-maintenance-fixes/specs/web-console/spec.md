# Spec Delta

## ADDED Requirements

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

When the login form is shown (initial load without a session, after logout, or after session end), the browser focus MUST move to the user name field by default.

#### Scenario: Open login

- **WHEN** a browser opens the console URL with no session and the login form appears
- **THEN** the user name field has keyboard focus

### Requirement: MQTT save applies without router restart

`POST /api/mqtt` MUST persist MQTT settings and apply them on the running host without restarting the router. The success response MUST NOT claim that the device will restart. Wi-Fi save MAY still restart.

#### Scenario: Save MQTT remote settings

- **WHEN** an admin saves valid MQTT settings
- **THEN** the settings are stored, MQTT client/broker behavior follows the new settings without a router restart, and the response does not announce a restart

## MODIFIED Requirements

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

### Requirement: ZigBee tab edits the Zigbee group

The ZigBee tab MUST show and allow saving CHANNEL and PERMIT_JOIN_ON_BOOT_SEC from the Zigbee group record. CHANNEL MUST be 11–26. PERMIT_JOIN_ON_BOOT_SEC MUST be 0–254. The page MUST load those values from `GET /api/zigbee` when the tab opens and on first page load, and MUST persist them with `POST /api/zigbee`. Saving MUST write EEPROM and apply the radio settings without restarting the router. Reset MUST reload stored values and MUST NOT write settings.

#### Scenario: Load Zigbee settings

- **WHEN** the operator opens the ZigBee section
- **THEN** CHANNEL and PERMIT_JOIN_ON_BOOT_SEC show the values stored on the host

#### Scenario: Save Zigbee settings

- **WHEN** the operator sets a valid channel and permit-join duration and saves
- **THEN** the Zigbee group is persisted and the radio settings are applied without a router restart

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
