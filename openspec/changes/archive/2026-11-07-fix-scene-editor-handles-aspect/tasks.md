## 1. Plan aspect and pointer mapping

- [x] 1.1 Replace non-uniform plan `toPx` / `worldFromClient` with a shared uniform scale (`min(width,height)`-based) so plan X/Z matches View
- [x] 1.2 Verify a square floor looks square in both plan and View on a non-square canvas

## 2. Selection and hit-testing

- [x] 2.1 Enforce exclusive single handle selection (one `selection` / one active drag handle)
- [x] 2.2 Invert near-tie priority in `findHandleAt` so wall/window/door endpoints win over floor corners in the same hit zone
- [x] 2.3 Add drag threshold: pointer-down selects without moving; move only after threshold; click-away does not relocate the handle

## 3. Floor Shift

- [x] 3.1 Change floor polygon Shift lock to use current adjacent vertices (not drag-start-only origin)
- [x] 3.2 Confirm horizontal and vertical Shift both lock correctly after uniform projection

## 4. Concave floor mesh

- [x] 4.1 Replace fan-from-first-vertex triangulation for floor faces with concave-safe triangulation (ear-clip or equivalent) so View/bake match plan fill
- [x] 4.2 Smoke: multi-room concave floor shows no diagonal spill outside walls in View

## 5. Embed and sanity

- [x] 5.1 Re-embed `data/js/scene.js` and confirm host `FIRMWARE_VERSION_HOST` build bump for this change
- [x] 5.2 Smoke: coincident wall/floor corner grabs wall; select then click empty leaves coordinates unchanged
