# Tasks

## 1. Hardware sticky panel

- [x] 1.1 Restructure `#system-tab-hardware` in `data/index.html` into a scrollable body and sticky Save/Reset actions footer; verify on a short viewport that fields scroll and both buttons stay visible.
- [x] 1.2 Add CSS for the Hardware panel flex/scroll/sticky footer in `data/css/all.css` without breaking other System tabs; verify Log still scrolls normally.

## 2. Automation editor UX

- [x] 2.1 Rebuild the automation edit dialog top into a **General** box (Name → Group → Active → Min trigger interval → Debug), remove Title, and space General / Timer / Triggered by devices / Condition / Actions by about one line; verify Title is gone and group spacing is visible.
- [x] 2.2 Add **Use condition** checkbox that shows/hides the condition editor and wires into draft read/write, import-empty checks, and Save payload; verify unchecking hides the editor and survives reopen after save once the store fields exist.
- [x] 2.3 Wire **Debug** checkbox into the draft and Save payload; verify it appears last in General and survives reopen after save once the store fields exist.

## 3. Host automation store and engine

- [x] 3.1 Add persisted `useCondition` and `debug` on `AutomationRecord` with JSON load/save defaults (`useCondition` true, `debug` false when omitted); clear/ignore Title from new editor saves; verify export/import and settings restore round-trip the new flags.
- [x] 3.2 Update validation so disabled condition does not require a condition tree; when `useCondition` is false, evaluation treats the condition as true; verify a timer-only automation with Use condition off runs actions on timer finish.
- [x] 3.3 When `debug` is true, log listener/trigger and condition comparison values via `LOGGER` on evaluation; verify System → Log shows those lines for a debug-enabled automation and stays quiet when Debug is off.

## 4. Apply version

- [x] 4.1 During apply, bump `FIRMWARE_VERSION` build by 1 from the propose value `3.1.13` and verify a host build succeeds for the environments that ship this console.

## 5. Follow-up editor polish

- [x] 5.1 Rename the Timer enable label to **Use countdown timer** in the automation edit dialog; verify the checkbox text shows the new wording.
- [x] 5.2 Wrap Triggered by devices heading, device rows, and **Add device to trigger** in one listen container (same chrome as General/Condition); verify they sit inside a single bordered group.
- [x] 5.3 Move the timer controls into the **Trigger** section at the beginning (before Triggered by devices) in one shared container; verify order is Use countdown timer then Triggered by devices.

## Workflow follow-up

- Archive after implementation review.
- Sync delta specs to main specs on archive/sync.
