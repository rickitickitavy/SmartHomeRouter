## Why

Scene bind editing still fights the operator in four ways: classic console Icon bindings lost their Scale control (regressed when scene pins switched to outline sizing), Export/Import live in the plan geometry editor instead of the bind editor, pin positions are normalized to the frame/window so they drift on the bake image when the viewport aspect changes, and gauge value text on scene pins is too small to read.

## What Changes

1. **Restore Icon Scale for classic cells** — The Binding dialog MUST again show **Icon scale** when View style is Icon for normal console bindings. Scene pin dialogs MUST keep Scale hidden (size stays via outline handles / `sizeW`/`sizeH` only). Classic cell icons MUST honor `iconScale` as before (viewport fit × `iconScale`).
2. **Move Export / Export scheme / Import** — Relocate those actions from the plan geometry editor (`Edit scene` geometry dialog) to the scene bind editor (dialog where the bake image and pins are managed). Geometry editor keeps Save/Cancel only for plan edits.
3. **Pin coordinates relative to bake image** — Pin place/drag/resize and overlay layout MUST use the bake image content box (`object-fit: contain` letterboxing), not the full frame or browser viewport, so the same pin stays on the same spot of the image across window sizes. Applies to scene bind editor and console Place / simple-user scene cells.
4. **Larger scene gauge text** — Value (and related) text inside scene pin / scene-cell gauges MUST be about **2×** the current size so readings are readable on the bake.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `scenes`: Bind editor export/import placement; pin coords vs bake image; scene gauge typography.
- `control-consoles` / `web-console`: Restore classic Icon scale field and rendering (scene pin exception unchanged).

## Impact

- `data/index.html` (and matching `data/user.html` scene-cell paths): bind dialog Scale field; scene bind vs geometry export buttons; image-relative pin mapping; gauge text CSS/sizing.
- `data/css/all.css`, `data/js/scene.js` as needed for overlay/image alignment and pin paint.
- Host firmware build bump only (`FIRMWARE_VERSION_HOST`); slave unchanged.
