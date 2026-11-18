# Proposal

## Why

Status, Interconnection, and history-storage UI still disagree between operator and simple-user pages, hide flash-persist on Repeater incorrectly, report zero remote device counts, and keep history settings mixed into Hardware. Operators also need a wider Search dialog and interconnect import that fully replaces paired Repeaters from the file.

## What Changes

- Restore **Persist historical on internal flash** visibility on Repeater (moved with other history settings to the new System tab).
- Compute and show real per-Repeater **device** counts on Main (devices whose `router_id` matches that peer).
- Remove **Devices** and **Online** fields from the paired-Repeater view dialog.
- Move interconnect **Queue: used / depth** from the Interconnection settings tab into the Status **Interconnection** widget.
- Rename Status history widget and rows: **Historical storage** → **Internal database**; **on-SD history size** → **on-SD size**; **Values stored** → **rows stored**.
- Show each Repeater’s host **VERSION** in the Status Interconnection widget.
- Rename General Status row **Slave** → **RF firmware** on the operator page so it matches the intended simple-user wording; keep both STATUS pages identical for these groups/labels.
- Widen the Search devices dialog so column headers and typical cell values fit on one line.
- At boot, if an SD card is present, automatically **disable** and **hide** Persist historical on internal flash.
- Add System nested tab **Internal DB** after **Hardware** and move all historical-data storage settings there.
- **BREAKING** for interconnect import/restore: paired Repeaters in Export settings / Interconnection import SHALL **fully override** the paired table from the file (no keep-existing-by-MAC authority).

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: Status widget labels/parity, System Internal DB tab, Search dialog width, peer dialog fields, queue placement.
- `router-interconnect`: live deviceCount, Status Interconnection queue + version rows, paired dialog field set, full paired-table override on import/restore.
- `historical-data`: Internal DB settings home; flash-persist auto-off/hide when SD is present; Repeater visibility of flash-persist control when applicable.

## Impact

- Host UI: `data/index.html`, `data/user.html`, `data/css/all.css`.
- Host firmware: `InterconnectStore` / status JSON device counts; `SettingsManager` / boot reconcile for flash persist vs SD; `WebConsole` Hardware vs Internal DB APIs and interconnect import flags; Status `/api/status` interconnect queue fields.
- No slave source changes expected.
- Host firmware build bump during propose.
