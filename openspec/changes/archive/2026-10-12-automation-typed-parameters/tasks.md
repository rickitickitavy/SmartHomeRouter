# Tasks

## 1. Host model and persistence

- [x] 1.1 Extend automation listener / condition comparison / device-action JSON with optional `param`, `attrId`, and (for actions) CUSTOM `cluster`/`command`/`value` fields; verify export/import and reboot round-trip including omitted legacy records.
- [x] 1.2 Add a shared known-parameter catalog (window covering Status/Position, On/Off Status, level Status+Light level, sensor value, battery, IAS status) mapping to cluster/attr or command/payload; verify keys resolve for each class listed in the design.
- [x] 1.3 Validate saves: CUSTOM listen/condition require cluster+attrId; CUSTOM actions require cluster+command; known params require a valid key for the device class; verify bad CUSTOM rows are rejected with a clear error.

## 2. Evaluation and commands

- [x] 2.1 Tighten listen matching so param/attrId-bound listeners only fire on that attribute/event; verify Position listener ignores Move Status-only updates and legacy cluster-only listeners still behave as before.
- [x] 2.2 Evaluate known-parameter conditions against interpreted status text and CUSTOM conditions against numeric VAL; verify EQUAL/GREATER/LESS on VAL and ON/OFF Status EQUAL.
- [x] 2.3 Execute known device actions via existing payloads and CUSTOM actions via cluster/command/value; verify On/Off ON and one CUSTOM command path succeed on a bound device.

## 3. Web console editor UX

- [x] 3.1 Rebuild Trigger rows: device, channel (hidden when single-channel), known parameter or CUSTOM (cluster+attrId), Remove with `btn-inline` matching Condition/Actions; verify single-channel hides channel and CUSTOM shows cluster/attrId.
- [x] 3.2 Apply the same picker to Condition comparisons (known vs CUSTOM VAL compare UI) and device Actions (CUSTOM shows cluster/command/value); verify window-covering Status/Position appear and Save persists typed fields.
- [x] 3.3 Align Remove (and related toolbar) button sizing across Trigger/Condition/Actions; verify Trigger Remove matches Actions Remove height/padding.

## 4. Apply version

- [x] 4.1 During apply, bump `FIRMWARE_VERSION` build by 1 from the propose value `3.1.20` and verify a host build succeeds for the environments that ship this console.

## Workflow follow-up

- Archive after implementation review.
- Sync delta specs to main specs on archive/sync.
