# Tasks

## 1. Queue module and settings

- [x] 1.1 Add `ACTION_COMMAND_QUEUE_MAX` (1024), reserved offset for the Hardware boolean (e.g. `reserved[7]`), and queue helpers module (PSRAM-backed static FreeRTOS FIFO, enqueue action / device-command jobs, busy count, overflow drop) — verify: compile; enqueue 1024 then drop; `busyCount()` matches
- [x] 1.2 Persist **use queue for base device command** (default false) via SettingsManager + GET/POST `/api/hardware` field `useQueueForBaseDeviceCommand` — verify: reboot keeps false/true; omit on POST → false
- [x] 1.3 Drain one FIFO job from host `loop()` with ≥100 ms spacing between starts — verify: two queued device jobs apply ≥100 ms apart

## 2. Automation and UI command producers

- [x] 2.1 Change `AutomationEngine` action run paths (`runActions` / `runAction` / trigger) to enqueue action snapshots instead of inline `applyAction`; drain calls existing apply — verify: trigger with two device actions spaces commands; email action still reaches `EmailSender` after drain
- [x] 2.2 Gate `applyManualDeviceCommand` / `POST /api/devices/command` on the Hardware flag (all device kinds); leave `/api/devices/configure` immediate — verify: flag off immediate ON; flag on queued ON/OPEN/custom; configure write never increments busy count
- [x] 2.3 Confirm console Run / widget automation actions use the enqueue path; Place + simple-user device widgets share the gated command path — verify: Run enqueues; widget OFF with flag on increases busy count

## 3. Status and Hardware UI

- [x] 3.1 Expose FIFO busy count in `hostGatewayStatusJson` and show it under Status **Free memory** on `data/index.html` and `data/user.html` — verify: both pages show matching busy count while jobs pending
- [x] 3.2 Add Hardware checkbox **use queue for base device command** (`.field-check`, square-only) with load/save — verify: admin can toggle and Save; non-admin still blocked from Hardware
- [x] 3.3 Bump `FIRMWARE_VERSION` build on apply (after propose `3.9.15`) when source lands; smoke host build — verify: version string in Status / boot log
