# Tasks

## 1. Detection and latch

- [ ] 1.1 Add host constant threshold `15 * 1024` and a once-per-boot latch checked from the main service path using `ESP.getFreeHeap()`; verify no trip above threshold and trip at/below in a controlled stress or test hook
- [ ] 1.2 Log the fail-safe trip when possible; verify a log line appears before teardown when LOGGER still works

## 2. Teardown and recovery sequence

- [ ] 2.1 On latch: stop nonessential host work (document concrete stops in code comments), then stop/destroy/free AsyncWebServer via a new `WebConsole` teardown; verify heap increases after teardown in a debug build or log
- [ ] 2.2 After teardown, call the current history snapshot persist (same as pre-restart), then `ESP.restart()` even if persist fails; verify reboot occurs and a successful persist restores history after boot when samples existed
- [ ] 2.3 Ensure Wi-Fi STA rebind cannot `server.begin()` after latch; verify no post-trip web restart before reboot

## 3. Host coverage

- [ ] 3.1 Confirm behavior is compiled for both `esp32-s3-host` and `esp32-p4-host`, slave untouched; verify host version **4.2.64** and slave version unchanged
