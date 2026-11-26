# Design

## Context

- See proposal.md — Why.
- Plan editor lives in `data/js/scene.js` (`SceneDesigner`).
- Today `onPointerDown` order is: handle hit → `selectNear` (walls/openings, then **floor polygon edges**) → `findFloorIndexAt` (point-in-polygon **fill**).
- Edge clicks set `selection = { list: "polygon", floorIndex }` and update `floorIndex` (works for Floor height / focus).
- Fill path sets `selection = { list: "floor", floorIndex }` via `findFloorIndexAt`, but operators report fill clicks do not select the floor — only edge (“rib”) clicks do.
- **Root cause (confirmed):** the same IIFE declares two `function pointInPolygon(...)` helpers. Function declarations hoist; the later camera-zone helper (`nx, ny, points` with `.x/.y`) shadows the plan helper (`point, polygon` with `.x/.z`). `findFloorIndexAt` therefore always gets a false miss on fill. Edge/handle paths never call `pointInPolygon`, so ribs still work.

## Goals / Non-Goals

**Goals**

- Clicking the painted floor plate (interior fill) selects that floor (focus + floor selection for height spinner / chrome).
- Top-most floor under the pointer wins when plates overlap.
- Handles and wall/window/door segments remain higher priority than floor fill.

**Non-Goals**

- Changing wall/handle vs floor-corner tie rules already specified.
- Changing Camera-zone polygon editing.
- Changing View/bake meshes.

## Decisions

1. **Unshadow plan `pointInPolygon`**  
   Rename one or both helpers (e.g. `pointInFloorPolygon` vs `pointInNormPolygon`) so `findFloorIndexAt` uses the XZ plan test again. Do not leave two same-named `function` declarations in the same IIFE.

2. **Treat floor fill as a first-class select target**  
   After handles and wall/opening segment hits, resolve floor selection with point-in-polygon fill via `findFloorIndexAt`. Result must be operator-visible floor selection (`floorIndex` + `list: "floor"` or equivalent accepted by `selectedFloor()` / Floor height).

3. **Keep wall/opening priority over fill**  
   Clicks near wall/window/door segments still select those entities first so floors never “own” walls.

4. **Edge vs fill**  
   Edge proximity may still select the floor (current rib behavior). Fill must not depend on being near an edge.

5. **Overlap**  
   Keep top-most drawn floor wins (iterate floors from highest index), matching paint order.

## Risks / Trade-offs

- [Dense walls inside a plate] → Interior clicks on a wall still select the wall; empty fill area must select the floor.  
- [Near-edge ambiguity] → Existing edge threshold may select `polygon` corner index; both must count as floor selected for height UI (`selectedFloor` already allows `floor` and `polygon`).

## Migration Plan

- Host UI/FS only; no store schema migration.

## Open Questions

- None; acceptance is: click fill → floor selected; click rib → still works; click wall → wall wins.
