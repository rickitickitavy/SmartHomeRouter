# Tasks

## 1. Rollback scene-as-widget draft code

- [x] 1.1 Remove `viewport: scene` / scene-widget Consoles chrome and ConsoleStore scene-widget fields from the incomplete apply; verify consoles still load vertical/grid/composite as before
- [x] 1.2 Keep history smooth (no EMA, Smooth off, samples 5) and account-tab full-screen on both pages; verify those still work after rollback

## 2. Role editScenes and users

- [x] 2.1 Add `editScenes` to user persistence, list/export/restore, Security dialog checkbox, and `isAdmin` implied rights; verify reboot and users table round-trip
- [x] 2.2 Gate scene mutate APIs and Scenes sidebar on `editScenes` or `isAdmin`; verify simple users cannot mutate scenes

## 3. Scene store and designer

- [x] 3.1 Implement host scene store (id, name, model, bake path, pins with relative x/y) with LittleFS bake files; verify save/load/reboot
- [x] 3.2 Build Scenes tab UI with list add/edit/delete and the Canvas designer (floors/walls/windows/doors, View scene, join, camera orbit, bake on Save); verify Cancel discards
- [x] 3.3 Scene pin dialogs (device/automation, Icon/Gauge, no titles, DnD relative coords); verify pins round-trip on the scene entity

## 4. Composite scene cells

- [x] 4.1 Add cell content `scene` to ConsoleStore parse/serialize and Consoles composite editor picker; verify save/reload
- [x] 4.2 Render scene cells on Place with bake + overlays and live pin controls; verify clicks match icon/gauge behavior
- [x] 4.3 Same scene-cell rendering on `user.html`; verify parity with Place
- [x] 4.4 Shared CSS for scene cell aspect box and overlays; verify no regression on other cell types

## 5. History and fullscreen (if not already complete)

- [x] 5.1 History chart: remove EMA; default Smooth unchecked; samples 5 on `index.html` and `user.html`
- [x] 5.2 Account-tab full-screen toggle beside Theme on both pages; session-only

## 6. Integration checks

- [ ] 6.1 Create a multi-floor scene with pins, embed it in a composite cell next to a camera cell, export/import, reboot; verify Scenes tab reopen and Place/user view
- [x] 6.2 User with only `editScenes` manages scenes but cannot edit consoles unless also `editConsoles`; assigned simple user views scene cell without Scenes tab

## Workflow follow-up

- Archive after implementation review.
- Sync delta specs to main specs on archive/sync.
