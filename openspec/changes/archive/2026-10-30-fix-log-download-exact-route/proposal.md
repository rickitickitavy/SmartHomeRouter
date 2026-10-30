# Proposal

## Why

**Download full log** has repeatedly returned only the last ~64 KiB even though the host ring and `/api/log/download` handler were written for the full used length. ESP32Async’s default URI matcher treats `/api/log` as matching `/api/log/...`, so the view-tail handler stole every download request. Operators still cannot get a complete log file.

## What Changes

- Register `/api/log` and `/api/log/download` with `AsyncURIMatcher::exact` (download first) so download always hits the full-ring handler.
- Keep on-screen Log at the existing ~64 KiB tail via `GET /api/log`.
- Document a regression scenario so parent/child log routes cannot be collapsed again.
- Bump `FIRMWARE_VERSION` build for this change (`3.9.14`).

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: Strengthen Log download so `GET /api/log/download` is never satisfied by the bounded view-tail route.

## Impact

- Host: `src/WebConsole.cpp` route registration; `include/Defines.h` version.
- No client HTML change required (button already targets `/api/log/download`).
- AsyncWebServer skill notes exact-matcher rule for parent paths with children.
