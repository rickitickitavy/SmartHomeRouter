# Tasks

## 1. Firmware version

- [x] 1.1 Bump `FIRMWARE_VERSION` build by 1 on apply (propose already set `3.5.64`) and verify `include/Defines.h` shows the apply build

## 2. Composite text round-trip

- [x] 2.1 Fix `extractJsonString` in `include/JsonField.h` so a match requires `"key"` followed by optional whitespace and `:`, skipping string values equal to the key name — verify with cell JSON ordered as `"content":"text","alignH":"center","text":"Hello"` that load yields display text `Hello`
- [x] 2.2 Manual: add widget → Composite → text cell → type non-empty text → Save → reopen console — verify the typed text appears (not `center`)

## 3. Widget editor Grid Columns/Rows visibility

- [x] 3.1 Ensure Columns/Rows stay hidden for Vertical and Composite (CSS `[hidden]` override and/or JS) — verify switching View style Vertical → Grid → Composite shows Columns/Rows only on Grid
- [x] 3.2 Confirm Columns/Rows still sit on one line with View style when Grid is selected — verify visually in the Consoles edit dialog

## 4. Binding dialog align for composite cells

- [x] 4.1 When `consoleCompositeEditTarget` is set, hide Horizontal and Vertical align in `#consoleDeviceDialog`; leave Vertical/Grid dialog behavior unchanged — verify composite Add/Edit device has no Align H/V while cell chrome still has them, and a Vertical binding still shows Vertical align

## 5. Enabled numeric input colors

- [x] 5.1 Style enabled `#consoleEditDialog` grid Columns/Rows and composite height/width (and related) inputs like enabled selects (`--text` / `--bg` / `--border`); keep gray only for disabled/readonly — verify Columns and row-height inputs no longer look grayed next to their dropdowns

## Workflow follow-up

- Archive the change after review and apply are complete.
