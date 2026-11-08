## MODIFIED Requirements

### Requirement: Scene designer and bake
The Scenes editor MUST provide top-view authorship for floors, walls, windows, and doors with the geometry rules previously defined for this change (defaults: sceneSize 1.0, wall height 0.15, wall length sceneSize/7; multi-floor; no furniture props). View scene MUST toggle 3D-style preview with camera orbit about the scene center. Save MUST commit the model and regenerate the bake; Cancel MUST discard editor changes. Plan edit projection MUST use a **uniform** X/Z scale (same world units to pixels on both axes, based on the lesser canvas dimension or equivalent), matching View scene world proportions. The plan MUST NOT stretch X by canvas width and Z by canvas height independently.

#### Scenario: Save regenerates bake
- **WHEN** the editor changes walls under View scene angles and activates Save
- **THEN** the stored model and bake update and Cancel is not required to keep those changes

#### Scenario: Cancel keeps prior bake
- **WHEN** the editor changes geometry and activates Cancel
- **THEN** the previously committed model and bake remain unchanged

#### Scenario: Plan and View share aspect
- **WHEN** the operator draws a square floor in plan and opens View scene
- **THEN** that floor appears square in View (same X/Z proportions as plan), not elongated or squashed by canvas aspect

#### Scenario: Concave floor matches plan in View
- **WHEN** the operator edits a concave multi-room floor polygon that fills correctly in plan and opens View scene (or regenerates bake)
- **THEN** the 3D floor plate follows that same outline and MUST NOT show diagonal spill triangles outside the polygon

## ADDED Requirements

### Requirement: Exclusive corner and endpoint selection
In plan edit mode, at most one geometry handle (floor corner or wall/window/door endpoint) SHALL be selected at a time. Starting a drag on a handle SHALL clear any other handle selection. Multi-select of corners/endpoints MUST NOT be offered.

#### Scenario: Selecting a second handle
- **WHEN** a floor corner is selected and the operator selects a wall endpoint
- **THEN** only the wall endpoint remains selected

### Requirement: Wall handle wins over floor in shared hit zone
When a wall endpoint (or shared wall corner) and a floor polygon corner fall within the same handle hit radius, pointer hit-testing MUST prefer the wall handle for select and drag.

#### Scenario: Coincident wall and floor corner
- **WHEN** a wall endpoint and a floor corner occupy the same plan position and the operator presses on that zone
- **THEN** the wall endpoint is selected/grabbed, not the floor corner

### Requirement: Select without relocating until drag
Pointer-down on a handle MUST select that handle without changing its world position. The handle MUST move only after the pointer moves beyond a small drag threshold while the button remains down. A click on empty plan (or elsewhere) after selection MUST NOT move that handle to the click location; it MAY clear selection or select another entity per existing segment/floor rules.

#### Scenario: Click select then click away
- **WHEN** the operator clicks a wall endpoint (no drag) then clicks empty canvas
- **THEN** the endpoint stays at its original coordinates

#### Scenario: Drag after threshold
- **WHEN** the operator presses a handle and moves past the drag threshold
- **THEN** the handle follows the pointer (with Shift rules when held)

### Requirement: Floor Shift locks to adjacent corners
While dragging a floor polygon corner with Shift held, axis lock MUST constrain motion using the **current** adjacent polygon vertices at that corner (the two neighbors), so horizontal lock keeps the corner’s Z aligned with the neighbor that defines the horizontal edge intent, and vertical lock keeps X aligned with the neighbor for the vertical edge—not a stale drag-start-only origin that disagrees with those neighbors after prior edits. Both horizontal and vertical Shift locks MUST function under the uniform plan projector.

#### Scenario: Horizontal Shift on floor corner
- **WHEN** the operator drags a floor corner with Shift and moves mostly along X
- **THEN** the corner’s Z stays locked to the appropriate adjacent corner’s Z and X follows the pointer

#### Scenario: Vertical Shift on floor corner
- **WHEN** the operator drags a floor corner with Shift and moves mostly along Z
- **THEN** the corner’s X stays locked to the appropriate adjacent corner’s X (current neighbor position) and Z follows the pointer

### Requirement: Concave-safe floor triangulation for View and bake
View scene and bake mesh construction MUST triangulate each floor polygon so the rendered plate matches the plan fill outline for concave (non-convex) simple polygons. Fan triangulation from the first vertex alone is NOT sufficient when the polygon is concave.

#### Scenario: No exterior diagonal plate
- **WHEN** a floor polygon has a concave outline under walls that form multiple rooms
- **THEN** View/bake MUST NOT draw floor triangles that cover area outside that polygon
