# Tasks

## 1. Exact log routes

- [x] 1.1 Register `GET /api/log/download` and `GET /api/log` with `AsyncURIMatcher::exact` (download first) in `WebConsole.cpp` and verify `/api/log/download` no longer matches the view-tail handler
- [x] 1.2 Keep `handleLogDownloadGet` on full `snapshotRing` and `handleLogGet` on `snapshotRingTail(kWebViewMaxBytes)`; verify download headers still expose ring capacity/used
- [x] 1.3 Bump `FIRMWARE_VERSION` build for this change and verify `include/Defines.h` shows `3.9.14`

## Workflow follow-up

- Sync delta specs to main, archive the change, commit, push, and open a PR.
