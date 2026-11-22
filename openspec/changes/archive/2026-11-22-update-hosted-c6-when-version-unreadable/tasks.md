## 1. Prepare decision

- [x] 1.1 In `FirmwareOta::prepareJoinedPackage` (P4): when `hosted-c6.bin` + valid `hosted-c6.version.txt` are present and `readRunningHostedC6Version` fails, set `hostedC6UpdateRequired = true`, log a warning, and continue (do not `fail` for unreadable running version)
- [x] 1.2 Keep fail on missing/empty package Hosted version text; keep skip-on-match and apply-on-mismatch when running version is readable

## 2. Specs and build

- [x] 2.1 Confirm delta wording matches joined-firmware-ota + p4-hosted-c6-ota after apply
- [x] 2.2 Build `esp32-p4-host` (S3 optional smoke); do not flash unless the human approves
- [x] 2.3 Confirm host **4.6.05**; slave unchanged; fragile-zone `firmware-update-process` approved before editing `FirmwareOta`

## 3. Smoke

- [x] 3.1 With Hosted version readable and matching package → still skip Hosted step
- [x] 3.2 With Hosted version unreadable (or forced 0.0.0) and package Hosted members present → Update proceeds to Hosted SDIO apply instead of failing prepare
