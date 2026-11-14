# Tasks

## 1. Hardware threshold setting

- [x] 1.1 Persist `lowMemoryThresholdKb` (10–32, default 16) in SettingsManager with hardware settings; GET/POST `/api/hardware` round-trip and reject out-of-range; include in settings export/restore
- [x] 1.2 Add **Low memory threshold** integer field on System → Hardware (operator UI); verify default 16 and validation 10–32

## 2. Detection and latch

- [x] 2.1 Add once-per-boot latch checked from the main service path using `ESP.getFreeHeap()` against `lowMemoryThresholdKb * 1024`; verify no trip above threshold and trip at/below in a controlled stress or test hook
- [x] 2.2 Log the fail-safe trip when possible; verify a log line appears before teardown when LOGGER still works

## 3. Teardown and recovery sequence

- [x] 3.1 On latch: stop nonessential host work (document concrete stops in code comments), then stop/destroy/free AsyncWebServer via a new `WebConsole` teardown; verify heap increases after teardown in a debug build or log
- [x] 3.2 After teardown, call the current history snapshot persist (same as pre-restart), then `ESP.restart()` even if persist fails; verify reboot occurs and a successful persist restores history after boot when samples existed
- [x] 3.3 Ensure Wi-Fi STA rebind cannot `server.begin()` after latch; verify no post-trip web restart before reboot

## 4. Host coverage

- [x] 4.1 Confirm behavior is compiled for both `esp32-s3-host` and `esp32-p4-host`, slave untouched; bump host firmware build on apply; leave slave version unchanged
