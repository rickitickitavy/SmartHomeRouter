# Proposal

## Why

When automations or the UI fire several device commands in a burst, part of the set often never arrives because work runs immediately on the trigger/HTTP path with no pacing. A bounded PSRAM FIFO that drains from the main loop at a minimum interval should space delivery for all device kinds and make queue pressure visible on Status.

## What Changes

- Add a host PSRAM action/command FIFO (capacity **1024**, static allocation allowed). New work that cannot fit is **ignored** (dropped).
- When an automation **action** must run (device, automation-control, or email enqueue path), **enqueue** it instead of executing inline; drain **FIFO** from the host main loop with at least **100 ms** between executions.
- Optionally route **base UI device commands** (Place/simple-user widgets, Devices table type actions, Manual command, console widget controls — all device kinds via the existing command API) through the **same** queue when a new Hardware setting is on.
- **Exclude** Devices **Configure** dialog / `/api/devices/configure` attribute writes from this queue (unchanged immediate path).
- System → Hardware: checkbox **use queue for base device command**, default **false**. Off → queue only for automation/console **actions**. On → queue also for base UI device commands.
- Status **Free memory** group: show this queue’s **busy count** (pending elements). Mirror on operator and simple-user Status.
- Propose bump: `FIRMWARE_VERSION` `3.9.14` → `3.9.15` (apply bumps again when source lands).

## Capabilities

### New Capabilities

- `action-command-queue`: Host PSRAM FIFO for deferred automation actions and optional UI base device commands; overflow drop; main-loop FIFO drain with ≥100 ms spacing; status busy count.

### Modified Capabilities

- `automations`: Action execution is deferred through the action-command queue rather than applied inline on the trigger path.
- `web-console`: Hardware GET/POST gains the queue-for-base-device-command flag; Status Free memory shows queue busy count.
- `control-consoles`: Click/run-action path always queues; live widget device commands follow the Hardware flag (same semantics as other base UI commands).

## Impact

- Host: new queue module (or equivalent), `AutomationEngine` / `runAction` enqueue, `applyManualDeviceCommand` / `POST /api/devices/command` gated by Hardware flag, `loop()` drain, `hostGatewayStatusJson` field, Settings/`reserved` bit + `/api/hardware`.
- UI: `data/index.html` Hardware checkbox + Free memory row; `data/user.html` Free memory row (no Hardware card on simple-user).
- Unchanged: `/api/devices/configure`, SPI/email lower-level queues, device kind coverage (all kinds when queued).
