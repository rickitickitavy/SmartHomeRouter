# Tasks

## 1. Status labels and Interconnection widget (both pages)

- [x] 1.1 Rename Status widget **Historical storage** → **Internal database**, **Values stored** → **rows stored**, **on-SD history size** → **on-SD size**, and General **Slave** → **RF firmware** in `data/index.html` and `data/user.html`; verify both pages show the same labels for the same payload
- [x] 1.2 Move interconnect **Queue: used / depth** into the Status Interconnection widget on both pages and remove it from the Interconnection settings tab; verify Status shows Queue and the settings tab does not
- [x] 1.3 Show each paired Repeater’s host VERSION in the Status Interconnection widget on both pages; verify a known peer version appears on Status

## 2. Paired table, peer dialog, Search width

- [x] 2.1 Compute and refresh `deviceCount` per paired Repeater from registered devices with matching `router_id`; verify Main Interconnection table shows non-zero counts when remote devices exist
- [x] 2.2 Remove **Devices** and **Online** from the paired-Repeater dialog markup/fill path; verify the dialog no longer shows those rows
- [x] 2.3 Widen the Search devices dialog/table CSS so column headers (especially Router) and typical cells fit on one line; verify against the reported layout issue

## 3. Internal DB tab and flash-persist rules

- [x] 3.1 Add System nested tab **Internal DB** after **Hardware** and move all historical-data storage settings (retention, slab/entry size, SD/flash persist and intervals) into it; verify Hardware no longer hosts those fields and Internal DB save/load still round-trips via existing hardware/history keys
- [x] 3.2 Stop hiding persist historical on internal flash solely because role is Repeater; verify Repeater Internal DB shows the control when SD is absent and size allows it
- [x] 3.3 At boot (and SD-active reconcile), when a usable SD card is present, force `historyPersistFlash` OFF, persist that, and hide the control; verify boot with SD leaves persist OFF and hidden

## 4. Interconnect export/import override

- [x] 4.1 Ensure global settings export includes paired Repeaters (already expected) and that settings restore plus Maintenance Interconnection import call full-override import (`keepExistingMacAuthoritative=false`); verify import/restore replaces the paired table from the file and removes peers absent from the file

## 5. Integration check

- [x] 5.1 Smoke-check operator vs simple-user Status parity (labels, Interconnection queue/version, Internal database rows) and System tab order Log → … → Hardware → Internal DB; verify no remaining Historical storage / Slave / Values stored / on-SD history size / settings-tab Queue strings on those surfaces
