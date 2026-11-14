# scenes Specification

## Purpose

Defines first-class Scene entities: schematic multi-floor plans with baked Full HD imagery, device/automation pins, a Scenes management tab, and `editScenes` authorization for create/edit/delete.

## Requirements

### Requirement: Scenes persist as first-class entities
The host MUST persist scenes with a unique id, name, editable scene model (geometry, colors, scene size, multi-floor heights, camera yaw/pitch), bake reference for a ≥ Full HD image, and an ordered list of pins. Each pin MUST target a device or automation, use relative coordinates in 0..1, and use Icon or Gauge styles per common console eligibility. Gauge pins MAY store optional title, `showTitle`, and `titlePosition` as specified for scene gauge titles; Icon pins MUST NOT require titles. After reboot the same scenes MUST load.

#### Scenario: Save and reload scene with pin
- **WHEN** an editor saves scene `Floor1` with a bake and a device pin at (0.4, 0.6)
- **THEN** after reboot that scene still has the model, bake, and pin at (0.4, 0.6)

### Requirement: Scene gauge pins support optional titles
A scene pin whose view style is **Gauge** MUST support optional title fields like console cell bindings: `showTitle` (boolean; default **false** when missing on scene pins so existing scenes stay untitled), optional title text, and `titlePosition` of `left`, `top`, `right`, or `bottom` (default `left` when shown and missing/invalid). When `showTitle` is false, live and preview rendering MUST NOT show a title. When `showTitle` is true, the title MUST appear relative to the gauge per `titlePosition`. Icon-styled scene pins MUST continue to omit titles. These fields MUST round-trip through scene save/load and exports that include pins.

#### Scenario: Gauge title top
- **WHEN** an editor saves a scene gauge pin with `showTitle` true, title `Kitchen temp`, and `titlePosition` `top`
- **THEN** after reload that pin still has those fields and Place/simple-user scene views show `Kitchen temp` above the gauge

#### Scenario: Show title off
- **WHEN** a scene gauge pin has `showTitle` false
- **THEN** Place and simple-user scene views show the gauge without a title label

#### Scenario: Legacy pin without title fields
- **WHEN** a stored scene gauge pin has no `showTitle` or `titlePosition`
- **THEN** it behaves as untitled (`showTitle` false) and still renders the gauge

#### Scenario: Icon pin stays untitled
- **WHEN** a scene pin uses Icon view style
- **THEN** no pin title is shown even if title fields are present in storage

### Requirement: Scene designer and bake
The Scenes editor MUST provide top-view authorship for floors, walls, windows, and doors with the geometry rules previously defined for this change (defaults: sceneSize 1.0, wall height 0.15, wall length sceneSize/7; multi-floor; no furniture props). View scene MUST toggle 3D-style preview with camera orbit about the scene center. Save MUST commit the model and regenerate the bake; Cancel MUST discard editor changes. Plan edit projection MUST use a **uniform** X/Z scale whose pixels-per-world-unit does **not** depend on canvas aspect ratio (fixed base × edit zoom / sceneSize). Resizing the editor window MUST enlarge or shrink the visible workspace (more or fewer grid cells) without stretching or compressing scheme geometry. The plan MUST NOT map X by canvas width and Z by canvas height independently. View/bake floor meshes MUST use concave-safe triangulation so floor plates match plan fill.

#### Scenario: Save regenerates bake
- **WHEN** the editor changes walls under View scene angles and activates Save
- **THEN** the stored model and bake update and Cancel is not required to keep those changes

#### Scenario: Cancel keeps prior bake
- **WHEN** the editor changes geometry and activates Cancel
- **THEN** the previously committed model and bake remain unchanged

#### Scenario: Plan and View share aspect
- **WHEN** the operator draws a square floor in plan and opens View scene
- **THEN** that floor appears square in View (same X/Z proportions as plan), not elongated or squashed by canvas aspect

#### Scenario: Workspace resize keeps scheme scale
- **WHEN** the operator widens the Edit scene window
- **THEN** the canvas grows to fill the workspace and shows more grid cells while object on-screen size stays the same (no stretch)

#### Scenario: Concave floor matches plan in View
- **WHEN** the operator edits a concave multi-room floor polygon that fills correctly in plan and opens View scene (or regenerates bake)
- **THEN** the 3D floor plate follows that same outline and MUST NOT show diagonal spill triangles outside the polygon

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

### Requirement: Scene geometry export and import
Export (full scene = house scheme plus pin bindings), Export scheme (house geometry and camera: floors, walls, windows, doors — no pins), and Import MUST be available on the **scene bind editor** (bake image and pins), not on the plan geometry editor. The plan geometry editor MUST keep Save/Cancel for geometry only. Import MUST remain enabled only when the imported target model is empty (default floor plate and no walls/openings), MUST auto-detect full vs scheme JSON, MUST restore pins when the file includes them, and MUST regenerate the bake preview after a successful import.

#### Scenario: Export lives on bind editor
- **WHEN** the operator opens the scene dialog used to place pins on the bake
- **THEN** Export, Export scheme, and Import controls are present there

#### Scenario: Geometry editor has no export/import
- **WHEN** the operator opens the plan geometry editor
- **THEN** Export / Export scheme / Import are not offered on that dialog

#### Scenario: Import disabled when not empty
- **WHEN** the editor already has walls or a non-default floor polygon
- **THEN** Import is disabled

#### Scenario: Scheme keeps walls
- **WHEN** the operator exports scheme from a scene that has walls and windows
- **THEN** the scheme JSON includes those walls and windows (not floors-only)

#### Scenario: Full export includes pins
- **WHEN** the operator exports a scene that has pin bindings
- **THEN** the full JSON includes `sceneModel` and `pins`

### Requirement: Pin coordinates relative to bake image
Scene pin positions (`x`, `y` in 0..1) MUST be relative to the **visible bake image content box** (the letterboxed rectangle produced by `object-fit: contain` or equivalent), not the outer frame, stage, or browser viewport. Place, drag, click-to-add, and resize math MUST use that image box. Overlay containers for pins MUST align to the same image box in the bind editor and in console Place / simple-user scene cells so pins keep their place on the image when the window aspect changes.

#### Scenario: Resize window keeps pin on image feature
- **WHEN** a pin is placed on a distinctive point of the bake and the operator resizes the browser so the stage aspect changes
- **THEN** the pin remains over that same point on the image (not shifted into letterbox empty space or stretched with the frame)

### Requirement: Readable scene gauge value text
Gauge value text rendered inside scene bind pins and console scene-cell pins MUST be approximately twice the prior scene-pin font size so values remain readable on the bake overlay.

#### Scenario: Scene gauge value is larger
- **WHEN** a scene pin uses a gauge view style
- **THEN** the numeric value text is about 2× as large as before this change while still fitting inside the pin box

### Requirement: Scene bind pin box aspect follows pin size ratio
Scene bind and console scene pin outlines MUST size width as a fraction of frame width and derive height from `sizeW`/`sizeH` aspect ratio so outline aspect does not repeat the viewport or bake image aspect ratio.

#### Scenario: Square icon pin stays square
- **WHEN** a pin has equal sizeW and sizeH on a wide bake frame
- **THEN** its outline is square on screen

### Requirement: Plan measurement grid major lines
The Edit scene plan grid MUST draw every fifth horizontal and vertical world-space grid line slightly bolder than the minor lines.

#### Scenario: Major lines every five steps
- **WHEN** the operator views the plan grid
- **THEN** every fifth line in each axis reads distinctly heavier than the intervening lines

### Requirement: editScenes gates scene management
Creating, updating, and deleting scenes MUST require `editScenes` or `isAdmin`. `isAdmin` MUST be allowed even when `editScenes` is false on the record. Users without those rights MUST NOT mutate scenes via API.

#### Scenario: editScenes can save a scene
- **WHEN** a session with `editScenes` and not `isAdmin` saves a scene
- **THEN** the host persists the scene

#### Scenario: Simple user cannot mutate scenes
- **WHEN** a simple user without `editScenes` posts a scene update
- **THEN** the host rejects the mutation

### Requirement: Console view of scenes does not require editScenes
Any signed-in user who may open an assigned console MUST be able to view scene cell content (bake and live pin controls) on Place or the simple-user page without holding `editScenes`.

#### Scenario: Assigned simple user sees scene cell
- **WHEN** a simple user opens an assigned console that contains a scene cell
- **THEN** the bake and pin overlays render and pin clicks use normal icon/gauge behavior
