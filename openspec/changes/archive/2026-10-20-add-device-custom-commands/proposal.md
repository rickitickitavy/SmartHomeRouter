# Proposal

## Why

Operators need per-device named commands (with an optional typed parameter) that work the same way from automations and from Place console widgets. Today automations only expose known cluster bindings plus ZCL CUSTOM; MQTT and Zigbee devices cannot declare reusable string commands, and Place device rows cannot act as a one-click action button with the full automation action editor.

## What Changes

- Each registered device MAY define up to **8** custom commands. Each command has a name (max **11** chars + NUL) and MAY require one parameter whose type is `int`, `bool` (payload `ON`/`OFF`), `float` (two decimal digits), or `string` (max **15** chars + NUL).
- When executed:
  - **MQTT** transport: command topic payload is `${command}` or `${command}=${value}` when a parameter is required.
  - **Zigbee** transport: command body is `cmd=${command}` when no parameter, or `cmd=${command},val=${value}` when a parameter is required (omit `val=` when unused — same omit-value pattern as this gateway’s existing ZCL `cl=…,cmd=…` bodies).
- Automation **device** actions MUST list that device’s custom commands in the command dropdown. If the selected command requires a parameter, the value field(s) MUST appear and MUST be filled before save/run.
- On Place (and simple-user) console widgets, a device binding MAY optionally attach a click action configured with the **same action editor as automations**. When set, that binding behaves as a custom action button: activating it in view mode runs that action. When unset, existing device control behavior is unchanged.
- Custom commands are settings: they MUST round-trip through device save, list/export, reboot, device edit UI, and automation/widget pickers. Add of a new command stays in-memory until the device Save (no pre-add orphans).

## Capabilities

### New Capabilities
- `device-custom-commands`: Per-device custom command definitions, limits, parameter types, and MQTT/Zigbee execute payload formats.

### Modified Capabilities
- `device-registry`: Persist custom commands on each registered device record through save/list/export/reboot.
- `automations`: Device actions include that device’s custom commands with required parameter UI/validation.
- `control-consoles`: Optional per-binding click action (automation-equivalent) that turns a Place device control into an action button.
- `web-console`: Device editor UI for custom commands; automation action dropdown/value fields; Places/user widget binding click-action editor and view-mode button behavior.
- `mqtt-device-topics`: Outbound path for executing device custom commands on MQTT vs Zigbee command topics/bodies.

## Impact

- Host device store / `DeviceTopicEntry` JSON (LittleFS devices file, list APIs, export/import).
- Automation action model and `AutomationEngine` execute path for named custom commands.
- Console binding schema and Place / `user.html` view-mode click handling.
- Operator Devices dialog and Automation action editor in `index.html`; Place binding editor.
- Firmware version build bumped to `3.5.44` for this propose.
