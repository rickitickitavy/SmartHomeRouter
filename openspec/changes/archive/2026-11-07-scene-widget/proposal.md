# Proposal

## Why

Operators need schematic floor/territory plans with placed device controls, reusable across consoles and combinable with cameras and other cell content on wide layouts. A console **widget type** for scenes blocks that composition. Scenes should be first-class entities, authored once, then embedded as composite **cell content**.

## What Changes

- Add a first-class **Scene** store (model, bake ≥ Full HD, device/automation pins with relative coordinates) and a **Scenes** sidebar tab with the scene designer (floors/walls/windows/doors, View scene, camera orbit, join, bake on Save).
- New user role **`editScenes`**: with it (or `isAdmin`), the user sees Scenes and can create/edit/delete scenes. Without it, no Scenes tab. Viewing a scene inside an assigned console remains available to **any** user who can open that console.
- Consoles: add composite cell content type **`scene`** (alongside empty/text/device/camera). A cell references a scene by id; live Place/`user.html` show bake + pin overlays with existing icon/gauge interaction.
- Keep in this change: account-tab **full-screen** toggle (not persisted); history chart **no EMA**, Smooth **unchecked** by default, samples default **5** (both pages).
- **Remove / do not ship** scene-as-widget (`viewport: scene`) approach from the prior draft of this change.

## Capabilities

### New Capabilities

- `scenes`: Scene persistence, designer, bake, pins, Scenes tab, and `editScenes` gating for management.

### Modified Capabilities

- `control-consoles`: Composite cell content may be `scene` (reference to a scene id); view rendering of scene cells.
- `console-users`: Persist and export role `editScenes`; `isAdmin` implies scene edit rights.
- `web-console`: Scenes section UI; composite cell scene picker; account full-screen; history smooth defaults/EMA removal; Place and simple-user scene cell parity.

## Impact

- New host scene store + LittleFS bake assets; user role field and Security UI checkbox.
- Operator Scenes tab; Consoles composite cell authoring; Place + `user.html` scene cell view.
- Export/import/settings restore include scenes and `editScenes`.
- Drop incomplete scene-widget viewport implementation from apply work; retain history/fullscreen UI fixes.
