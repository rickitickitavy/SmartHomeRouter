# Proposal

## Why

A status-triggered automation (e.g. `alarm button` on a leak detector) runs once after boot; later matching reports show debug compare true but do not run again until reboot. Root cause today is a status-path rising-edge latch on the condition. The product rule the operator wants is not “always re-fire” and not “always one-shot”: each **trigger device** needs an explicit **Event type** — default **Change** (value must change) vs **Any value** (re-evaluate even when the reported value is unchanged).

## What Changes

- Persist per listening entry an `eventType`: `change` (default) or `any` (UI labels **Change** / **Any value**). Omitted JSON means `change`.
- **Change:** that listener triggers evaluation only when the bound status value actually changes (no same-value re-fire for that listener).
- **Any value:** that listener may trigger evaluation on re-reports of the same value; when the condition is true, actions may run again without requiring a false→true condition edge for that evaluation (still subject to `minTriggerIntervalSec`).
- Automations editor **Triggered by devices** rows: show field labels on the same line as each control (including **Field** for the parameter); place **Event type** at the **end** of the row (after Field / custom cluster-attr controls, before Remove).
- Remove the accidental global “status always needs rising-edge” one-shot that blocks re-runs when Event type is **Any value**; keep rising-edge for current-time polling.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `automations`: listening entries gain `eventType`; evaluation semantics for Change vs Any value; condition rising-edge no longer blocks Any-value re-reports.
- `web-console`: Triggered-by-devices row labels on the same line as controls; Event type control last; default Change.

## Impact

- Host `AutomationStore` / `AutomationEngine`, `data/index.html` automation listen editor, export/import JSON.
- `FIRMWARE_VERSION` build already `3.5.09` at propose; apply bumps again when source lands.
- Existing automations without `eventType` behave as **Change** (value must change).
