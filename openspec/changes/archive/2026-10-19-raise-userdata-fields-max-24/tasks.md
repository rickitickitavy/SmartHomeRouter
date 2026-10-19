# Tasks

## 1. Firmware max and persistence bound

- [x] 1.1 Set `kUserDataFieldMaxCount` to 24 in `UserDataPayload.h` — verify: brace-slot helpers accept 24 slots
- [x] 1.2 Raise `userDataFieldsJson` copy bound from 768 to a shared constant (~2048) used in `DeviceTopicMap` / `WebConsole` / `main` — verify: a 24-field schema with long names saves and reloads intact
- [x] 1.3 Bump `FIRMWARE_VERSION` build by 1 on apply (propose `3.5.16`) — verify: `include/Defines.h`

## 2. Web console

- [x] 2.1 Change `normalizeUserDataFields` and `addUserDataFieldRow` caps from 16 to 24 — verify: Add allows 24 rows and does not add a 25th; normalize keeps at most 24

## 3. Soak

- [ ] 3.1 Manual: save `userData` device with 24 fields, reload, bind one field in automation/console — verify: all 24 persist; pickers list all names
