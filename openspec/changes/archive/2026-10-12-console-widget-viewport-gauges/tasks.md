# Tasks

## 1. Persist model

- [x] 1.1 Extend `ConsoleWidget` / `ConsoleBinding` in `ConsoleStore` for viewport, gridColumns/gridRows, param/cluster/attrId, viewStyle, and nested gauge (type/min/max/ranges); verify host parse of a sample JSON object with and without the new keys
- [x] 1.2 Serialize new fields in console full JSON / export paths and omit defaults where practical; verify round-trip save → load → export matches for grid+gauge and for legacy vertical-only JSON
- [x] 1.3 Confirm settings export/import and console import of a layout that includes viewport and gauge; verify reload after reboot keeps values

## 2. Consoles editor UI

- [x] 2.1 Add widget Viewport (Vertical / Horizontal / Grid) and conditional Columns/Rows (default 2×2) in the Consoles edit dialog; verify preview updates on change and Cancel discards unsaved edits
- [x] 2.2 Extend the console device-binding dialog with automation-listen-style device/channel/param/custom cluster+attrId; verify dependent selects match automation “device to trigger” behavior
- [x] 2.3 Add View style Default/Gauge and Gauge group (arc/linear, min/max, color-range list with Top + color picker) for numeric/custom bindings; verify On/Off, covering, and automation modes hide gauge UI
- [x] 2.4 Wire read/write of new fields into dialog baseline dirty-check and Save body; verify Save persists and reopening the dialog restores them

## 3. View-mode layout and gauges

- [x] 3.1 Implement vertical / horizontal / grid binding layout CSS and DOM in `user.html` and Place view; verify grid fill L→R top→bottom and overflow to extra rows
- [x] 3.2 Implement HA-like arc and linear gauge rendering that scales to cell/slot; verify clamp at min/max for draw and fit without overlapping neighbors
- [x] 3.3 Resolve live numeric values via ieee+channel+param (custom cluster/attr when set); verify gauges update with status and missing values show empty/unavailable without breaking the widget

## 4. Integration check

- [x] 4.1 Manual pass: create console with mixed On/Off + gauge bindings, all three viewports, Save, reload, simple-user page and Place; verify no device commands from editor preview and FIRMWARE_VERSION build reflects apply bump beyond propose `3.2.10`
- [x] 4.2 Host build for the shipping environment (`pio run` for host env used in this project) succeeds after source changes

## Workflow follow-up

- Archive the change after review (`/opsx-sync` then `/opsx-archive`, or combined archive with sync).
