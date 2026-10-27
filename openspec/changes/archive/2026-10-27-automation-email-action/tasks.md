# Tasks

## 1. Email settings model and API

- [x] 1.1 Add `EmailSettings` to `GlobalSettings`, bump settings version 6→7 with v6 upgrade (email defaults), and verify `sizeof(GlobalSettings) ≤ 4096` and reserved alignment still pass host fixtures
- [x] 1.2 Implement SettingsManager defaults/load/save for the email group and `isEmailConfigured()` matching the outbound-email gate; verify GET after Save round-trips fields and unconfigured returns false when host or From empty
- [x] 1.3 Add `GET`/`POST /api/email` on WebConsole (admin session) and include `email` in settings export/restore; verify export contains email keys and restore reloads them

## 2. Async SMTP sender

- [x] 2.1 Add host-only ESP-Mail-Client (or equivalent) dependency in `platformio.ini` and verify the host env builds with the new lib
- [x] 2.2 Implement `EmailSender` (queue + worker task) that sends using stored account settings; verify a configured test enqueue returns immediately and success/failure appears in System → Log
- [x] 2.3 When account is not configured or enqueue fails, log a warning and do not block; verify no SMTP connect attempt when unconfigured

## 3. Automation email action (store + engine)

- [x] 3.1 Extend `AutomationAction` with email target fields and PSRAM alloc/free helpers; verify email-action strings allocate from PSRAM and device actions still use internal heap
- [x] 3.2 Parse/serialize email actions in AutomationStore (receivers, theme, message, ≤8 parameters with device/attribute/title); verify save/list/export/import/reboot round-trip and >8 parameters are rejected
- [x] 3.3 In AutomationEngine, on email action: if unconfigured skip+warn and continue; else resolve live values, build body (`message` then `title: value` lines), enqueue async send; verify body lines and that a following device action still runs after skip

## 4. Web console Network E-mail + action editor

- [x] 4.1 Add Network → **E-mail** tab and E-Mail card (all account fields, Save/Reset, password show/hide) wired to `/api/email`; verify load/save/reset behaviors match MQTT-style Network cards
- [x] 4.2 Add automation action target **e-mail** UI (receivers, theme, message, parameter rows with device/attribute/title, max 8); verify switching targets shows/hides fields and saved automation reloads the email action in the dialog
- [x] 4.3 Bump `FIRMWARE_VERSION` build on apply (after propose `3.7.15`) when source lands; verify version string in Status / boot log

## 5. Integration check

- [ ] 5.1 End-to-end: configure SMTP, create automation with email action + two parameters, trigger it on STA, and verify mail arrives (or SMTP log success) with correct subject/body lines without freezing the console
- [x] 5.2 Confirm fragile zones `slave-spi-ota` and `firmware-update-process` were not edited

## Workflow follow-up

- Archive the change after review when implementation is complete (`/opsx-archive`).
