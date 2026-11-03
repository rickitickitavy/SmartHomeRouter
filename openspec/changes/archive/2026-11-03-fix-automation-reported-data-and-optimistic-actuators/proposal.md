# Proposal

## Why

Automation conditions and listeners can pick **Reported data** (`reportable:cluster/attr`), but the host rejects those saves (generic “need timer, or a listener plus a complete condition…”), while **Sensor value** works. Separately, Place and simple-user actuator controls (switcher / icon, including group On/Off) stay on the old state until a live report arrives, so the UI feels hung after a command.

## What Changes

- Accept **Reported data** param keys on automation listeners and condition comparisons: save validation, persistence, trigger matching, and condition comparison against the host’s live reportable value (same converted text widgets already show).
- Raise the automation param key length limit as needed so `reportable:0xXXXX/0xXXXX` is not truncated.
- **Optimistic actuator UI (approach A)**: when the operator toggles an actuator via switcher or icon (including each On/Off member of a group toggle), update that control’s displayed state **immediately**, wait up to **5 seconds** for a confirming live status for that device/channel, and **revert** the display if no confirming status arrives. Apply the same behavior on Place and the simple-user page. A later confirming (or contrary) live status still wins after the wait.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `automations`: Reported-data (`reportable:…`) bindings are first-class for listen and condition; they must save, trigger, and compare correctly.
- `control-consoles`: View-mode actuator switcher/icon (and group On/Off per member) use optimistic immediate feedback with a 5s confirm-or-revert window.
- `web-console`: Place view-mode matches the same optimistic actuator behavior as the simple-user page (sync rule).

## Impact

- Host: `AutomationParam.h` / `AutomationStore` validation and param key size; `ZigbeeSpiProxy::copyEndpointStateForParam` (and related reportable-value lookup); `AutomationEngine` listen match / condition resolve for reportable params.
- UI: `data/index.html` (Automation + Place) and `data/user.html` (simple-user consoles) for optimistic actuator rendering.
- HTTP save error text may stay the same for other invalid cases; reported-data saves must succeed when otherwise valid.
- Firmware build bumped to `3.9.71` for this propose.
