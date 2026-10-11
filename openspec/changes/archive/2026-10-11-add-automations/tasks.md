# Tasks

## 1. Store and model

- [x] 1.1 Add heap `AutomationStore` (`/automations.json`, max 32, heap floor like Consoles) with name, title, active, TIMER persisted fields (`use`, `autoRestart`, `defaultTime`), listeners, condition tree, actions; verify save/list/reboot round-trip and 33rd create fails; verify `counting` / `timeLeft` are not written to LittleFS.
- [x] 1.2 Implement JSON serialize/parse for conditions (AND/OR/XOR, NOT, nesting caps), device actions, and automation actions (ENABLE/DISABLE; timer ops only when target may use timer); enforce validation (`!use` → ≥1 listen + ≥1 condition; `use` allows empty); verify invalid trees/actions are rejected on save.

## 2. Runtime engine

- [x] 2.1 In host status cache update, detect endpoint/state string change (ignore identical writes and online-only churn); verify same-value re-report does not invoke the engine.
- [x] 2.2 TIMER tick: decrement active counting automations; on zero evaluate; auto-restart when configured; on `active` FALSE→TRUE with `use`+`autoRestart`+`defaultTime` start countdown; verify timer-only (no listen/condition) runs actions on finish.
- [x] 2.3 Evaluate on status-change (listener + optional cluster) or timer finish; resolve comparisons from cache; verify AND/OR/XOR/NOT and compare ops; empty condition with `timer.use` is TRUE.
- [x] 2.4 On true condition, run actions in order: device commands via existing applicator; automation ENABLE/DISABLE and STOP/START/RESTART TIMER; set RAM last-activation; verify inactive never fires and last-activation / timer runtime clear after reboot.

## 3. HTTP APIs and settings

- [x] 3.1 Add admin-only `/api/automations` list/create, item get/put/delete, store export and restore; GET MAY include lastActivation and timer runtime; verify non-admin 403.
- [x] 3.2 Include `automations` in settings export/restore (replace when key present, omit leaves store); verify export omits last-activation and TIMER runtime.

## 4. Web console UI

- [x] 4.1 Sidebar: insert **Automation** after Devices (admin-only); move **Appearance** after Consoles; verify admin order and non-admin hides Automation.
- [x] 4.2 Automation list panel: name, active, last performed; Add / Export all / Import all; verify refresh after save/import.
- [x] 4.3 Edit dialog: name, title, active; **Timer** (`use`, `autoRestart`, `defaultTime`, show counting/timeLeft); Listen / Condition / Actions; Actions: Device | Automation target with ENABLE/DISABLE and timer ops when target uses timer (START TIMER optional time); Save/Cancel; Export `<name>.json`; Import only when empty (including timer unused); verify nested condition and timer fields round-trip through GET after Save.

## 5. Apply version

- [x] 5.1 While applying this change, increment `FIRMWARE_VERSION` build by 1 from the then-current value and verify host `pio run` for the host env succeeds.

## Workflow follow-up

- Archive the change after review and implementation are complete.
