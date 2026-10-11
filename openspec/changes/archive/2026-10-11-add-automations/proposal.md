# Proposal

## Why

Operators need host-side automations: when selected devices’ status values *change*, and/or when an optional countdown timer finishes, evaluate a condition tree and, if true, run device commands or control other automations. Today there is no rule engine on the gateway; every reaction requires MQTT external logic or manual clicks.

## What Changes

- Add host-persisted **automations** (name, title, active, optional TIMER, listening devices, condition tree, action list) in LittleFS; evaluate and run **only on the Wi‑Fi host** (master). Slave remains Zigbee radio only.
- Trigger a condition check when a listening device’s relevant status value changes (identical re-reports MUST NOT evaluate), **or** when an enabled TIMER countdown reaches zero. Optional per-listener cluster filter; optional comparison cluster (else device main/default status).
- Complex attribute **TIMER**: `use`, runtime `counting` and `timeLeft` (seconds), `autoRestart`, `defaultTime`. When `use` is true, countdown finish is a valid evaluation trigger. When `active` goes FALSE→TRUE and `autoRestart` is true and `defaultTime` is set, load `defaultTime` and start countdown. After finish + evaluation, if `autoRestart` and `defaultTime` are set, reload and count again.
- Validation: if TIMER `use` is false → at least one listener and at least one condition required. If `use` is true → listeners and conditions MAY be empty; empty conditions evaluate TRUE (timer-only automations).
- Condition model: **comparison** nodes and **container** nodes (AND / OR / XOR over children, each node has NOT). Comparisons: device, channel (multichannel), optional cluster, op (EQUAL, GREATER, LESS, CONTAINS, STARTS, ENDS), compare value.
- Actions: ordered list. Each action targets a **device** (existing command model: `ieee` + payload + channel) **or** an **automation** (`ENABLE`, `DISABLE`; if that automation uses a timer also `STOP TIMER`, `START TIMER`, `RESTART TIMER`). `START TIMER` MAY supply countdown seconds when the target has no `defaultTime`.
- Runtime-only **last activation** and TIMER `counting` / `timeLeft` (not settings persistence); last activation shown in the Automations list when known.
- Web console: new main sidebar **Automation** immediately after **Devices**; move **Appearance** to after **Consoles**. List: name, active, last performed; Add / Export all / Import all. Edit dialog with Timer, Listen, Condition, Actions; Export and empty-only Import; single-file export named by automation `name`.
- Settings export/restore includes automations. Firmware build bumped to `1.0.24` for this propose.

## Capabilities

### New Capabilities

- `automations`: Persist, edit, import/export, and host-run automations (TIMER, listeners, condition trees, device and automation actions, change- and timer-triggered evaluation).

### Modified Capabilities

- `web-console`: Automation sidebar after Devices; Appearance after Consoles; Automations list/dialog UX (including TIMER and automation-target actions); settings export/restore includes automations group.

## Impact

Host: new store (e.g. `/automations.json`), evaluation on device status updates and timer tick, reuse of existing device-command path plus in-process automation enable/disable/timer ops, `WebConsole` REST + `data/index.html` sidebar/panels. No slave Zigbee protocol change. User role for the Automation tab: `isAdmin` (same class as WiFi/MQTT/ZigBee). Cap and heap floor follow ConsoleStore-style patterns (design).
