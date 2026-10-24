# Tasks

## 1. Device custom command model and persistence

- [x] 1.1 Add `DeviceCustomCommand` constants/struct (max 8, name ≤11+NUL, optional param type) and attach a heap list on `DeviceTopicEntry`; verify compile and sizeof/caps match design
- [x] 1.2 Implement clone/free/parse/serialize of `customCommands` in device JSON (`listStoreJson`, `entryJson`, upsert/replace); verify a fixture with two commands round-trips and omitted field loads as empty
- [x] 1.3 Reject saves with >8 commands, empty/duplicate/too-long names, or invalid paramType in WebConsole/device save path; verify API rejects and store unchanged
- [x] 1.4 Confirm slave SPI sync payloads still omit custom commands and host pull does not wipe them; verify after a sync cycle `customCommands` remain on the host record

## 2. Execute path (MQTT / Zigbee)

- [x] 2.1 Implement shared execute helper that formats MQTT `${cmd}` / `${cmd}=${value}` and Zigbee `cmd=` / `cmd=,val=` with bool ON/OFF and float two decimals; verify unit-style checks or log traces for each encoding
- [x] 2.2 Wire helper into the host device-command path with channels mapping per `mqtt-device-topics`; verify MQTT device publishes only and Zigbee uses the body path (Manual command / automation runner)

## 3. Automations

- [x] 3.1 Extend automation action model to reference a device custom command by name without colliding with ZCL `hasCustomCommand`; verify export/import of an action with `reboot` and with `level=40`
- [x] 3.2 Update `AutomationEngine` to resolve and execute device custom commands via the shared helper; verify MQTT and Zigbee actions fire the expected payloads
- [x] 3.3 Validate automation save requires a typed value when the selected custom command has a parameter; verify empty value is rejected and filled value accepted

## 4. Operator Devices UI

- [x] 4.1 Add custom-commands editor on the device parameter dialog (Add/Edit/Remove, type controls, caps); verify Add is memory-only and Cancel leaves store empty
- [x] 4.2 Persist commands only on device Save and refresh list JSON; verify after Save the commands appear in list/export and survive reload
- [x] 4.3 Ensure Enabled command editor controls are not gray (enabled-controls rule); verify visually against disabled/readonly styling

## 5. Automation action UI

- [x] 5.1 Include the selected device’s saved custom commands in the action command dropdown; verify they appear only for that device after a successful device Save
- [x] 5.2 Show typed value fields when the custom command requires a parameter and require them on Save; verify bool/int/float/string controls and no-param hides the requirement

## 6. Place / console click action

- [x] 6.1 Persist optional `clickAction` on `ConsoleBinding` (same shape as one automation action); verify console export/import/reboot round-trip and legacy bindings load without a click action
- [x] 6.2 In Place binding editor, offer optional click action using the shared automation action editor; verify Cancel/discards and Save persists; edit mode does not run the action
- [x] 6.3 In Place and `user.html` view mode, render bindings with click action as action buttons and run the action on activate; verify normal controls still work when click action is absent, and edit mode still sends no commands
- [x] 6.4 Add or reuse an API to run a click-action JSON from view mode; verify one successful Place click performs the same side effect as the equivalent automation action

## 7. Integration checks

- [x] 7.1 End-to-end: define commands on MQTT and Zigbee devices, use them in an automation, and as a Place click action; verify payloads and UI after reboot
- [x] 7.2 Confirm firmware build is `3.5.47` in Status/version API after the implementation build

## 8. Color picker presets

- [x] 8.1 Set gauge color-picker presets Red/Green/Blue/Yellow to the specified RGB values and add Violet; verify swatches in binding dialog

## Workflow follow-up

- Archive the change after review when implementation is complete (`/opsx-archive`).
