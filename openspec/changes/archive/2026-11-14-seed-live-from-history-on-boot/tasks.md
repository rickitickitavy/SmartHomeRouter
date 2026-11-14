# Tasks

## 1. Live-cache silent seed API

- [x] 1.1 Add a host API on `ZigbeeSpiProxy` (or equivalent) that writes reportable live slots without calling `historyNote*` and without invoking `statusValueChanged` / automation hooks; verify a unit or host-side call updates cache JSON and does not append history
- [x] 1.2 Extend silent seed for historized main-state / endpoint status text using the same format as live reports; verify device-list status JSON shows the seeded payload and no automation enqueue occurs

## 2. History → live seed after restore

- [x] 2.1 Expose a way to walk restored series with a last sample (`hasLast` / typed last value, fallback to newest record if needed) and map series keys to IEEE/endpoint/cluster/attr; verify enumeration matches restored snapshot series that have samples
- [x] 2.2 After successful `restoreSnapshot` + `syncSeriesFromDevices` in host boot, call silent seed for each eligible series; verify Place/simple-user and device-list show values before a new report, and history `valuesStored` does not rise solely from seeding
- [x] 2.3 When no snapshot / no last sample, skip seeding; verify live fields stay empty until a real report

## 3. Automation non-fire and overwrite

- [x] 3.1 Confirm seed path never calls status-path listeners; verify an automation listening on a seeded attribute does not enqueue on boot seed alone
- [x] 3.2 Confirm a real report after seed still updates live cache, may append history under existing rules, and may trigger automations under existing event-type rules; verify with one reportable change listener

## Workflow follow-up

- Archive the change after review when implementation is complete.
- Sync delta specs into main specs if archive does not already.
