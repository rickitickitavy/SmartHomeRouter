# Proposal

## Why

Repeater consoles still expose Internal DB (history storage) that belongs on Main only; Status Network does not show interconnect packet totals; and there is no operator time-zone setting, so Main and Repeaters cannot share a consistent UTC offset for wall-clock features.

## What Changes

- Hide System → **Internal DB** when network role is **Repeater** (admins on Repeater no longer see or use that tab).
- On Status **Network** (operator and simple-user), add two totals: **Repeater packets sent** and **Repeater packets received** (sum of paired peers’ interconnect `packetsOut` / `packetsIn` on Main; sensible zeros/omission when not Main or no peers).
- Add System → Hardware **Time zone** as a **UTC offset** chosen from a **fixed list with 30-minute steps** (e.g. `UTC−12:00` … `UTC+14:00`). Persist with hardware settings and include in global settings export/restore.
- Main owns the offset: saving on Main applies locally and pushes to all paired Repeaters over interconnect. Repeaters apply the Main-pushed value and MUST NOT offer an independent editable time-zone control (read-only display allowed).

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: Internal DB visibility on Repeater; Status Network repeater packet rows; Hardware time-zone control and Repeater read-only behavior; Status parity for Network rows.
- `router-interconnect`: Main pushes time-zone offset to Repeaters; status/API exposure of aggregated repeater packet counters for Status Network.

## Impact

- Host UI: `data/index.html`, `data/user.html` (Status Network).
- Host firmware: Hardware API / `SettingsManager` for UTC offset minutes; NTP/`configTime` apply; `RouterInterconnect` variable push/apply; status JSON packet totals.
- No slave source changes expected.
- Host firmware build bump during propose.
