# Proposal

## Why

Operators hit several small console UX gaps at once: missing gauge color, cramped Grid size fields, inconsistent delete controls, no easy row reorder for Grid/Composite, widget automation bindings that can only toggle Enable/Disable (not Run or timer actions), binding wording, app viewport chrome gaps, and theme/sign-out split across Appearance vs Sign out. This change packs those fixes so every signed-in user can pick a theme, sign out cleanly, and drive a bound automation from the widget with the right control.

## What Changes

- Add a **Cyan** gauge color-picker preset at RGB `(59, 130, 246)` (`#3b82f6`).
- Put widget **Columns** and **Rows** (with labels) on one horizontal line with the **View style** dropdown, in the same container; keep showing them only when View style is Grid.
- Replace composite **Remove row** text with the recycle-bin icon button already used elsewhere for delete.
- Make the recycle-bin icon the default remove/delete control pattern; document that in the Web UI skill.
- In the widget editor, when View style is **Grid** or **Composite**, add small **up/down arrow** buttons to move rows (Grid: binding/cell order; Composite: composite rows).
- For a **console widget automation binding** only: closed dropdown of control choices (no free typing). Default **Enable/Disable** (existing `active` toggle). Plus every action available for that selected automation (**Run**, **Start timer**, **Stop timer**, **Restart timer**, … when TIMER is used). Non–Enable/Disable choices use a **button**. If the automation is inactive: name gray, icon opacity 90%. After a non–Enable/Disable click: icon shows ON for **0.2 s**. Does **not** change automation-editor action rows between automations.
- Rename widget **Add binding** to **Add cell**.
- Remove top and bottom margin/padding from the global app viewport chrome (flush content to the top and bottom of the shell; keep left/right as designed).
- Replace the sidebar **Sign out** control with a tab labeled with the **current user name**. That tab’s card holds **Sign out** and the **theme** controls currently on Appearance. **Remove the Appearance** main section. Simple users get an equivalent trailing tab (user name) with the same theme + Sign out content.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: color presets; widget Grid field layout; Remove row → recycle bin; Grid/Composite row up/down arrows; Add cell rename; global viewport margins; Appearance removal; account tab with theme + Sign out; widget automation control UI.
- `control-consoles`: persist widget automation binding control mode; view-mode Enable/Disable vs Run/timer button; editor row reorder for Grid/Composite.
- `automations`: host accepts operator-initiated **Run** and timer ops from console widget controls. No change to automation-editor inter-automation action op list.

## Impact

- UI: `data/index.html`, `data/user.html`, `data/css/all.css`.
- Firmware: binding control-mode persistence; WebConsole API for Run/timer from view mode; `FIRMWARE_VERSION` propose `3.5.51`, bump again on apply.
- Skills: `~/.cursor/skills/web-ui/SKILL.md` — recycle-bin default for remove/delete.
