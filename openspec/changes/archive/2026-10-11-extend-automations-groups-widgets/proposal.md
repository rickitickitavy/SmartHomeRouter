# Proposal: extend automations (groups, widgets, start-on-boot edge)

## Why

Operators need clearer Status memory labeling, grouping of automations for large lists, console widgets that can enable/disable automations like On/Off devices, and timer start-on-boot to also arm when an automation becomes active—not only twenty seconds after reboot.

## What Changes

- When TIMER `use` and `startOnBoot` are true, a FALSE→TRUE change of `active` MUST start the countdown from `defaultTime` (when `defaultTime` > 0), in addition to the existing post-boot delayed start.
- Automations gain a persisted optional `group` string. Empty group is shown as **Not in group**. The Automation section gets a narrow left group list (**All** first) that filters the automations table.
- Control-console widgets MAY bind automations. Bound automations show an On/Off control mapped to the automation’s `active` flag; the user MAY toggle it in view mode (same interaction model as On/Off device rows).
- Status Memory MUST report and label **free** RAM/PSRAM (not used), e.g. Free memory / Free RAM / Free PSRAM with free kB and free percent.

## Capabilities

### New Capabilities

_(none)_

### Modified Capabilities

- `automations`: `group` attribute; `startOnBoot` also starts timer on active edge; list/filter semantics for groups.
- `control-consoles`: widget bindings may target automations with On/Off (`active`) controls.
- `web-console`: Automation list UI with group picker + group field in edit dialog; Status Memory free labeling/values; console editor supports automation bindings.

## Impact

- Host: `AutomationStore` / `AutomationEngine` (active edge + group persist/API), `ConsoleStore` (binding model + JSON), gateway status JSON, WebConsole APIs if needed for toggling automation active from the user page.
- UI: admin Automation section layout, automation edit dialog, console edit dialog and `user.html` widget rendering.
- Settings export/import must round-trip `group` and automation bindings.
- FIRMWARE_VERSION build bumped to `1.1.05` at propose time.
