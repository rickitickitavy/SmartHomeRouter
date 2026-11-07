## Context

Plan editing in `data/js/scene.js` maps world `(x,z)` to canvas with **non-uniform** scale (`toPx` uses `width` for X and `height` for Z). View/Three.js uses uniform world XZ, so the same model looks differently proportioned (operator screenshots: plan vs View). `findHandleAt` currently **prefers floor** on near-ties over walls—the opposite of the requested priority. Floor Shift uses `axisLockPoint(dragCornerOrigin, …)` (pointer-down origin); combined with non-uniform `worldFromClient`, same-pixel horizontal vs vertical motion yields unequal world deltas, so horizontal Shift often loses the axis pick. Selecting a handle starts `dragHandle` immediately on pointer-down, so click-away / tiny slips can relocate geometry. Separately, `faceToThreeGeometry` fans triangles from the first vertex; plan uses `ctx.fill()` on the path, so concave multi-room floors look correct in plan but spill diagonal plates in View/bake.

## Goals / Non-Goals

**Goals:**
- Exclusive single-handle selection.
- Wall endpoint wins over floor corner in the same hit zone.
- Drag threshold: select without move; no teleport on click-away.
- Uniform plan projection matching View aspect.
- Floor Shift axis-lock from current adjacent polygon vertices; horizontal and vertical both work.
- View/bake floor mesh matches plan floor outline for concave polygons.

**Non-Goals:**
- Changing wall Shift snap-to-floor-corner behavior except as needed for consistency with uniform projection.
- Auto-deriving floor polygons from walls (floors stay independent plates).
- Redesigning tools, bake resolution, or multi-floor UX beyond hit/drag/aspect/floor mesh.
- Slave firmware changes.

## Decisions

1. **Uniform plan scale** — Introduce one `planScale = min(canvasWidth, canvasHeight) * editZoom / sceneSize` (or equivalent). Use it in both `toPx` and `worldFromClient` so round-trip stays consistent and X/Z pixels-per-unit match. Grid drawing may stay decorative.

2. **Hit priority** — In `findHandleAt` near-tie logic, prefer `walls`/`windows`/`doors` over `polygon` (invert today’s `preferFloor` bias). Still pick closer handle when clearly nearer.

3. **Drag threshold** — On pointer-down: set selection + remember start client position; do not apply world moves until movement exceeds ~4–6 CSS pixels (or ~0.3× handle hit size). Before threshold, pointer-up = select only. After threshold, existing drag path runs. Click empty: clear handle drag state without calling move/snap with a new point for the old handle.

4. **Floor Shift reference** — For `handle.list === "polygon"`, compute axis lock against neighbors `polygon[(i±1) % n]`: choose the dominant axis from pointer delta; lock the other coordinate to the neighbor that best matches that axis (e.g. for horizontal intent, lock Z to the neighbor with closer Z, or the neighbor of the edge more aligned with X). Do not use only `dragCornerOrigin` when neighbors define a clearer axis. Walls keep existing Shift (floor-corner snap then axis-lock to the wall’s other end).

5. **Single selection** — Keep one `selection` / one `dragHandle`; ensure UI never paints multiple corner handles as selected.

6. **Concave-safe floor triangulation** — Replace fan-from-`pts[0]` for floor faces (or for all planar faces with >3 verts) with ear-clipping / equivalent concave-safe triangulation in XZ (Y constant). Wall quads stay 4-point fans. Keep floors independent of walls; only fix how `floor.polygon` becomes triangles.

## Risks / Trade-offs

- **Plan will look different** on non-square canvases (letterboxed uniform scale vs today’s stretch). That is intentional so View matches plan.
- **Drag threshold** adds a few pixels of slack before move—acceptable for click-select.
- **Self-intersecting floor polygons** may still triangulate poorly; out of scope unless operators report bow-ties.

## Migration Plan

- No persisted model migration. Existing scenes keep coordinates; only editor display/interaction changes.
- Re-embed `scene.js` with host web assets; bump `FIRMWARE_VERSION_HOST` only.

## Open Questions

None blocking. Assumption: shared wall-corner group move (`moveSharedWallCorner`) remains one logical endpoint for wall edits.
