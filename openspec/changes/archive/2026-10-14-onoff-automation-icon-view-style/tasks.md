# Tasks

## 1. Binding dialog View style for On/Off and automation

- [x] 1.1 Extend `refreshConsoleDeviceGaugeFields` (and any `updateConsoleBindingTargetFields` path) so View style shows Default/Icon when target is automation or selected device is `onOff` (switch-eligible param), not only binary — verify: dialog for On/Off and automation shows View style; Icon reveals the MDI picker; Gauge is absent
- [x] 1.2 Persist `viewStyle`/`icon` for automation bindings in `readConsoleDeviceDialogBody` (and keep On/Off device save when Icon selected) — verify: save then reopen dialog restores Icon + chosen `mdi:…` for both On/Off and automation

## 2. Interactive icon render on Place and user pages

- [x] 2.1 In `placeBuildDeviceRow` (and Consoles edit preview), when automation or On/Off binding has `viewStyle === "icon"`, render the MDI icon control instead of `.console-switch`, with ON/active yellow and OFF/inactive muted; wire the same toggle handlers as the switch; preview remains display-only — verify: Place view toggles device/automation via icon; edit preview does not send commands
- [x] 2.2 Mirror the same icon-vs-switch branching and handlers in `data/user.html` — verify: simple-user page shows icon and toggles On/Off / automation active
- [x] 2.3 Adjust `data/css/all.css` only as needed so interactive icons are clearly clickable (cursor/focus) without changing binary display-only icons’ look — verify: icon controls look active like switches; binary icons unchanged visually

## 3. Integration check

- [x] 3.1 Smoke: Default switch still works for On/Off and automation; Icon path works after page reload; group header On/Off remains a switch; binary Icon still display-only — verify: all four behaviors observed on Place and user pages
