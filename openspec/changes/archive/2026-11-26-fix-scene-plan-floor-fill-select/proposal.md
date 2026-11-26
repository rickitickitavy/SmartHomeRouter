# Proposal

## Why

In the Plan scene geometry editor, clicking inside a floor polygon (the filled plate) does not select that floor. Selection only works when clicking the polygon’s edges (“ribs”). Operators expect a fill click to select the floor the same way an edge click does.

## What Changes

- Make floor **fill** hit-testing select the floor in plan edit (set focused floor / floor selection).
- Keep existing priority: handles and wall/window/door segments still win over floor fill when those are the click target.
- Do not change View/bake, Camera scenes, or pin editing.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `scenes`: Plan editor floor selection MUST succeed on polygon fill clicks, not only on edges/corners.

## Impact

- UI: `data/js/scene.js` plan pointer hit-test / selection path (`onPointerDown`, `selectNear`, `findFloorIndexAt`).
- Host firmware build bumped to **4.6.43** at propose (web FS bundle).
- No slave changes.
