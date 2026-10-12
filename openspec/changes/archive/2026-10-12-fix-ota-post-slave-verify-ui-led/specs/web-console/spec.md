# Spec Delta

## ADDED Requirements

### Requirement: Firmware Update dialog result plates stay visible

Success and error plates inside the firmware Update dialog (`firmwareRebootSuccess` and `firmwareRebootError`) MUST remain visible until the operator closes that dialog or starts a new firmware update. Those plates MUST NOT be hidden by the global ten-second auto-hide used for other `save-error` / `save-success` plates.

#### Scenario: Reboot success plate stays

- **WHEN** the Update dialog shows the reboot-wait success plate after the router comes back
- **THEN** that plate stays visible past ten seconds until the operator closes the dialog

#### Scenario: Reboot error plate stays

- **WHEN** the Update dialog shows the reboot-wait error plate after the wait times out
- **THEN** that plate stays visible past ten seconds until the operator closes the dialog

#### Scenario: Other plates still auto-hide

- **WHEN** a settings Save shows a success or error plate outside the firmware Update dialog
- **THEN** that plate still hides after about ten seconds as today

## MODIFIED Requirements

### Requirement: Error and success plates hide after ten seconds

Whenever the web console shows an error plate (`save-error`) or success plate (`save-success`), including login errors, that plate MUST become hidden automatically after 10 seconds unless a newer message replaces it sooner. Hiding MUST NOT clear unrelated form fields. Plates inside the firmware Update dialog that report the reboot-wait request result are exempt and MUST follow the firmware Update dialog result-plate requirement instead.

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

- **WHEN** the firmware Update dialog shows its reboot-wait success or error plate
- **THEN** that plate is not auto-hidden after ten seconds
