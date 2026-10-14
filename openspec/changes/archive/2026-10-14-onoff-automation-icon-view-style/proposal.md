# Proposal

## Why

Console widgets already support Material Design Icons for binary sensors, but On/Off device rows and automation rows are always rendered as switches. Operators want the same View style Icon option for On/Off and automations, with the chosen icon remaining clickable (toggle ON/OFF or automation active) like today’s switch.

## What Changes

- Enable **View style** for On/Off device bindings and automation bindings: `default` (switch, as today) or `icon` (MDI icon picker).
- When View style is `icon`, Place and user console pages MUST show the selected icon instead of the switch, with ON/active vs OFF/inactive colors matching the binary-icon theme (yellow active, muted inactive).
- The icon MUST be **active**: tapping it toggles the device (ON↔OFF) or the automation `active` flag the same way the switch does today.
- Reuse the existing binding `viewStyle` / `icon` persistence and MDI icon catalog where practical.
- Widget header group On/Off control stays a switch (out of scope unless later requested).
- No **BREAKING** API changes; missing view style remains switch/default.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `control-consoles`: On/Off and automation bindings may use view style `icon`; icon control is interactive like a switch.
- `web-console`: Consoles binding dialog shows View style Default/Icon for On/Off and automation targets; Place/user pages render and activate icon controls.

## Impact

- Web UI: `data/index.html`, `data/user.html`, `data/css/all.css` (icon-as-button affordance if needed).
- Host console store already supports `viewStyle: icon` + `icon`; extend dialog eligibility and render paths for `onOff` and automation bindings.
- Firmware version bumped to **3.4.02** for this propose.
