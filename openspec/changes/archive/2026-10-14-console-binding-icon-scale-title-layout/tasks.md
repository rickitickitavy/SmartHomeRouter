# Tasks

## 1. Persist new binding fields

- [x] 1.1 Extend `ConsoleBinding` / `ConsoleStore` parse + JSON emit for `iconScale`, `showTitle`, and `titlePosition` with defaults `1` / `true` / `left` — verify: save console JSON, reboot or reload API, fields round-trip for device and automation bindings
- [x] 1.2 Bump is already at propose `3.4.04`; ensure apply keeps a single build bump if further source edits land on a different base — verify: `FIRMWARE_VERSION` reflects the implementation build

## 2. Binding dialog

- [x] 2.1 Add Show title checkbox and Title position select to the Consoles binding dialog for device and automation targets; persist via `readConsoleDeviceDialogBody` — verify: reopen dialog restores both fields for On/Off, binary, gauge, and automation
- [x] 2.2 Add Icon scale field visible only when View style is Icon; save/restore `iconScale` — verify: field hidden for Default/Gauge; Icon path saves `1.5` and reloads it

## 3. Render Place, user, and preview

- [x] 3.1 Apply title show/position classes (or equivalent flex layout) in `placeBuildDeviceRow`, Consoles edit preview, and `user.html` `buildBindingRow` — verify: left/right/top/bottom and hidden title match on Place and user pages
- [x] 3.2 Apply `iconScale` as a multiplier on viewport-fit icon size (CSS variable or equivalent) for icon controls without changing non-icon controls — verify: `iconScale` `2` in a grid cell is larger than `1` in the same cell; viewport fit still applies when scale is `1`
- [x] 3.3 CSS for title-position modifiers and icon scale composition — verify: no regression to group On/Off switch or binary display-only colors

## 4. Integration check

- [x] 4.1 Smoke: defaults match old layout; icon On/Off/automation still toggle; showTitle false + titlePosition bottom + iconScale 1.25 survive page reload — verify: observed on Place and user pages

## 5. Content align for all view styles

- [x] 5.1 Persist `alignH` / `alignV` on device and automation bindings (legacy `iconAlign` → `alignH`); emit non-defaults in console JSON — verify: Default-view On/Off with left/top survives reload
- [x] 5.2 Show Horizontal and Vertical align in the binding dialog for Default/Icon/Gauge; remove Icon-only Icon align; save/restore via dialog body — verify: Default On/Off dialog shows both fields and reopens saved values
- [x] 5.3 Apply align classes on Place, user, and editor preview for all view styles; CSS for cell/slot alignment — verify: Default grid binding bottom-right differs from center
- [x] 5.4 Bump `FIRMWARE_VERSION` build by 1 with this source change — verify: version string increased
- [x] 5.5 When Show title is unchecked, Consoles edit preview still shows a muted registered device name (not Title, not IEEE) on the plate; load `/api/devices` when opening the editor so the name resolves — verify: Default On/Off with showTitle false shows e.g. “Light in birdbarn”, never the IEEE string
- [x] 5.6 When Show title is checked, hide Horizontal align and keep classic row layout (title left/right, control on the same line); apply alignH only when title is hidden — verify: Default On/Off with title shows name and switch on one row
