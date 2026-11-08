## Why

Scene geometry editing still fights the operator: overlapping floor and wall handles prefer the floor, a select-click can relocate a handle, floor Shift-lock is unreliable (especially horizontal), View scene does not match the plan’s X/Z proportions because the plan projector stretches axes independently to the canvas, and View/bake draws concave floor plates with a vertex fan so the 3D floor outline does not match the plan fill.

## What Changes

1. **Single handle** — At most one corner/endpoint is selected and drag-active at a time (exclusive selection).
2. **Wall over floor** — When a wall endpoint/corner and a floor corner share the same hit zone, pick/grab the wall handle.
3. **Select ≠ move** — Pointer-down on a handle selects it; the handle moves only after a drag threshold. Clicking elsewhere must not teleport the selected endpoint.
4. **Uniform plan aspect** — Plan edit view uses the same uniform X/Z scale as world/View scene (fix non-uniform `width`/`height` mapping in `toPx` / `worldFromClient`), so View matches the plan’s proportions.
5. **Floor Shift** — Shift axis-lock for floor corners uses the **adjacent polygon vertices** (current opposite ends of the edges at that corner), not a stale drag-start origin; horizontal and vertical locks both work under the uniform projector.
6. **Concave floor mesh** — View scene and bake MUST triangulate floor polygons so the 3D plate matches the plan outline (no fan-from-first-vertex diagonals that spill outside concave rooms).

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `scenes`: Designer pointer selection/drag rules, plan↔View aspect parity, floor Shift snap reference, concave-safe floor triangulation for View/bake.

## Impact

- `data/js/scene.js` (and re-embed): `findHandleAt` tie-break, drag threshold, plan projection, `resolveShiftDragPoint` / floor Shift, `faceToThreeGeometry` / floor mesh build.
- Scene bake/View should follow corrected plan world mapping and the same floor outline as plan fill.
- Host firmware build bump only (`FIRMWARE_VERSION_HOST`); slave unchanged.
