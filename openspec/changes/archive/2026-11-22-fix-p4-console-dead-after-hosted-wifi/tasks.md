## 1. Confirm P4 Hosted recovery gap

- [x] 1.1 Trace current ESP-Hosted / wifi-remote APIs and sdkconfig restart options used by `esp32-p4-host`; list which signals mean transport or remote Wi‑Fi reinited
- [x] 1.2 Confirm S3 shares the same `requestStaWebRebind` machine and must not gain Hosted-only arms
- [x] 1.3 Document in apply notes how field logs show Hosted-arm vs STA-event-arm vs polled-arm

## 2. P4: arm existing web rebind on Hosted recovery

- [x] 2.1 Under `BOARD_HOST_P4`, when Hosted recovery is observed and STA mode is active (not SoftAP), call `requestStaWebRebind()` (same gates as today: armed, `!apActive`, STA boot path)
- [x] 2.2 Keep `WebConsole::rebind()` execution only via `runPendingStaWebRebind` in `WiFiController::update()` / host loop — never from Hosted/Wi‑Fi event callbacks
- [x] 2.3 Preserve first-boot bind: do not arm until `setInterfaceReadyHandler` has armed rebind after `WebConsole::begin()`
- [x] 2.4 If no reliable Hosted hook exists, implement the design backup (narrow P4 detect → same `requestStaWebRebind`); do not add a short-period HTTP self-probe as the default

## 3. Logging and smoke

- [x] 3.1 Add distinct P4 log lines when rebind is armed from Hosted recovery vs STA disconnect/GOT_IP vs polled disconnect
- [x] 3.2 Build `esp32-p4-host` (and confirm S3 host still builds without behavior change); do not flash unless the human approves
- [x] 3.3 Smoke plan: after Hosted/C6 recovery with live ping/MQTT, expect rebind log and console HTTP; classic STA disconnect/GOT_IP path still rebinds; SoftAP path unchanged

## 4. Versions

- [x] 4.1 Confirm propose host bump **4.5.20**; leave slave **4.2.18** (host-only change)
- [x] 4.2 Bump host build again on apply if host sources change further
