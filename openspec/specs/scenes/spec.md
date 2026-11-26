# scenes Specification

## Purpose

Defines first-class Scene entities: schematic multi-floor Plan scenes with baked Full HD imagery and device/automation pins, Camera scenes with refreshing JPEG backgrounds and active control zones, Custom background scenes with operator-uploaded still images on SD and Plan-like pins, a Scenes management tab, and `editScenes` authorization for create/edit/delete.

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

### Requirement: Each floor has its own height
Each floor MUST store its own positive height. Default for a new floor MUST be **0.01**. There is no upper bound on floor height (value MUST be greater than zero). Heights MUST persist with the scene. In View/bake, each floor MUST be rendered with that height (prism). All floors MUST share the same base height (MUST NOT be vertically stacked at different start levels). Wall height remains a separate scene control for walls.

#### Scenario: Per-floor heights persist
- **WHEN** the operator sets different heights on two floors and saves
- **THEN** after reload each floor still has its own height

#### Scenario: Floors share base height
- **WHEN** two floors have different heights and View scene is open
- **THEN** both floors start at the same base height

#### Scenario: New floor default height
- **WHEN** the operator adds a floor
- **THEN** that floor’s height is `0.01` when no other value is chosen

### Requirement: Floor height spinner
The scene editor MUST provide a separate Floor height spinner immediately after the Add floor button, without a text label (arrows only). The spinner MUST be enabled only when a floor is selected, MUST display that floor’s height, and MUST edit only that floor. When no floor is selected, the spinner MUST be disabled.

#### Scenario: Placement
- **WHEN** the operator opens Edit scene
- **THEN** the Floor height spinner is immediately after the Add floor button

#### Scenario: Enabled when floor selected
- **WHEN** a floor is selected
- **THEN** the Floor height spinner is enabled and shows that floor’s height

#### Scenario: Disabled when no floor selected
- **WHEN** no floor is selected
- **THEN** the Floor height spinner is disabled

### Requirement: Geometry Add buttons are not toggles
Add floor, Add wall, Add window, and Add door MUST be ordinary buttons and MUST NOT appear selected/active/outlined as toggles.

#### Scenario: Click Add wall
- **WHEN** the operator clicks Add wall
- **THEN** the button does not stay outlined as selected/active

### Requirement: Scene designer and bake
The Scenes editor MUST provide top-view authorship for floors, walls, windows, and doors with the geometry rules previously defined for this change (defaults: sceneSize 1.0, wall height 0.15, per-floor height default 0.01, wall length sceneSize/7; multi-floor; no furniture props). View scene MUST toggle 3D-style preview with camera orbit about the scene center. Save MUST commit the model and regenerate the bake; Cancel MUST discard editor changes. Plan edit projection MUST use a **uniform** X/Z scale whose pixels-per-world-unit does **not** depend on canvas aspect ratio (fixed base × edit zoom / sceneSize). Resizing the editor window MUST enlarge or shrink the visible workspace (more or fewer grid cells) without stretching or compressing scheme geometry. The plan MUST NOT map X by canvas width and Z by canvas height independently. View/bake floor meshes MUST use concave-safe triangulation so floor plates match plan fill.

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

### Requirement: Scene pin arc digit and title proportions
Scene-pin arc gauges MUST use the operator-approved digit-to-arc proportion from `Screenshot from 2026-11-15 21-31-18.png` and, when a title is shown, the label hierarchy from `Screenshot from 2026-11-15 21-32-10.png` (label clearly smaller than digits, about one-third digit height). Those proportions MUST hold as the scene/pin scales across viewport/resolutions.

#### Scenario: Matches approved scene digit look
- **WHEN** Place shows a scene pin arc gauge at a normal desktop size
- **THEN** the center digits match the visual proportion of `Screenshot from 2026-11-15 21-31-18.png`

#### Scenario: Matches approved scene label look
- **WHEN** Place shows a titled scene pin arc gauge (e.g. `температура на улице`) at a normal desktop size
- **THEN** the label matches the hierarchy of `Screenshot from 2026-11-15 21-32-10.png`

#### Scenario: Stable across resolutions
- **WHEN** the same titled scene pin gauge is viewed at a large desktop window and at a much smaller window
- **THEN** digit-to-arc and label-to-digit proportions stay at the approved looks while the arc scales

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

### Requirement: Scene kind Plan or Camera
Each scene MUST have kind `plan`, `camera`, or `custom`. Missing kind on stored scenes MUST behave as `plan`. Camera scenes MUST use a registered camera as background and active zones. Plan scenes MUST keep bake-image and pin authoring. Custom background scenes (`custom`) MUST author pins like Plan and MUST use an operator-uploaded still image as the background instead of a geometry-generated bake. Camera scenes MUST NOT use icon/gauge pins in the operator or simple-user UI. Custom background scenes MUST NOT use Camera zones.

#### Scenario: Legacy scene is Plan
- **WHEN** a stored scene has no kind field
- **THEN** it behaves as Plan (bake + pins)

#### Scenario: Camera scene has no pins UI
- **WHEN** the operator opens a Camera scene for live view or bind editing
- **THEN** pin placement/controls are not offered; active zones are used instead

#### Scenario: Custom background uses pins like Plan
- **WHEN** the operator opens a Custom background scene for bind editing or live view
- **THEN** icon/gauge pin authoring and display work like Plan, and Camera zones are not offered

### Requirement: Custom background image storage and limits
A Custom background scene’s background image MUST be stored on the SD card (not LittleFS). Upload MUST accept PNG and JPEG still images (and equivalent lightweight formats the host serves as files) with a maximum body size of **1 MiB**. Upload MUST fail with a clear error when SD is missing or unusable, when the body exceeds 1 MiB, or when the format is not accepted. Successful upload MUST update the scene’s bake/image reference so Place and simple-user can load it.

#### Scenario: Upload stores on SD
- **WHEN** the operator uploads a valid ≤1 MiB JPEG for a Custom background scene and SD is usable
- **THEN** the image is stored on the SD card and subsequent bake/image GET returns that image

#### Scenario: Upload rejected without SD
- **WHEN** the operator uploads an image for a Custom background scene and SD is missing or unusable
- **THEN** the host rejects the upload with a clear error and does not write the image to LittleFS

#### Scenario: Oversized upload rejected
- **WHEN** the operator uploads an image larger than 1 MiB for a Custom background scene
- **THEN** the host rejects the upload

### Requirement: Custom background has no geometry editor
Custom background scenes MUST NOT open the Plan geometry **Edit scene** flow. Background replacement MUST be via upload only. Changing kind away from `custom` MUST follow existing kind-switch rules without inventing Plan geometry tools on Custom scenes.

#### Scenario: Edit scene hidden for Custom background
- **WHEN** scene kind is Custom background
- **THEN** the Edit scene (geometry) control is not shown

### Requirement: Camera scene background refresh
A Camera scene MUST show the selected camera’s JPEG background and MUST refresh it every scene `refreshInterval` seconds. `refreshInterval` MUST be per-scene, default **3**, and MUST clamp to **1..60** seconds when saved/applied.

#### Scenario: Default refresh
- **WHEN** a new Camera scene is created without an explicit refresh interval
- **THEN** the background refreshes every 3 seconds

#### Scenario: Custom refresh
- **WHEN** the operator sets refresh interval to 10 and saves
- **THEN** after reload the Camera scene refreshes every 10 seconds

### Requirement: Active zones
A Camera scene MUST support ordered active zones. Each zone MUST store a polygon (normalized coordinates in the visible camera image box), transparency (0..1), optional ON/OFF colors, a binding that drives ON/OFF state from a device/reportable value (every binary / On-Off-like state including binary sensors), and optional **Action for ON** / **Action for OFF** automations. When transparency is less than 1 (not fully transparent), ON and OFF colors MUST be specified. When transparency is 1, colors are not required. On zone click: if the current state is ON and Action for ON is set, run that automation instead of sending OFF; if the current state is OFF and Action for OFF is set, run that automation instead of sending ON; if the action for the current state is empty, send the usual device command (On/Off actuators) or do nothing for non-command binary sensors. At design time, zone fill MUST render at **70%** transparency regardless of the stored transparency so zones remain editable. Zone placement and size MUST scale with the scene image box.

#### Scenario: Design-time visibility
- **WHEN** the operator edits zones on a Camera scene
- **THEN** each zone is shown at 70% transparency even if stored transparency is 1

#### Scenario: Runtime colors
- **WHEN** a zone has transparency 0.4, colorOn green, colorOff red, and the bound value is OFF
- **THEN** Place/simple-user show the zone filled red at the configured transparency

#### Scenario: Click ON-state automation replaces OFF command
- **WHEN** the bound device state is ON, Action for ON is an automation, and the operator clicks the zone
- **THEN** that automation runs and OFF is not sent to the device

#### Scenario: Click with no state automation uses device command
- **WHEN** the bound On/Off device state is OFF, Action for OFF is empty, and the operator clicks the zone
- **THEN** the usual ON command is sent to the device

#### Scenario: Fully transparent still clickable
- **WHEN** a zone has transparency 1 and an Action for ON automation
- **THEN** the zone has no required fill colors and a click with current state ON still runs that automation

### Requirement: Zone polygon editing without Shift
Camera-scene zone polygon editing MUST follow the same vertex create/move/select pattern used for floor polygons on the plan editor, except Shift axis-lock MUST NOT be applied.

#### Scenario: No Shift lock
- **WHEN** the operator drags a zone vertex with Shift held
- **THEN** motion is not forced onto a single axis by Shift lock

### Requirement: Plan editor selects floor by polygon fill click
In the Plan scene geometry editor, a pointer press on the **filled interior** of a floor polygon MUST select that floor (focused floor and floor selection used by Floor height and related chrome). Selection MUST NOT require clicking only a polygon edge or corner. When multiple floor plates overlap under the pointer, the top-most drawn floor MUST win. Wall/window/door segment hits and geometry handles MUST still take priority over floor fill when the press is on those targets.

#### Scenario: Click floor fill selects floor
- **WHEN** the operator presses inside a floor polygon’s filled area (away from handles and wall/opening segments)
- **THEN** that floor becomes the selected/focused floor

#### Scenario: Click floor edge still selects floor
- **WHEN** the operator presses near a floor polygon edge (“rib”)
- **THEN** that floor is selected as today

#### Scenario: Wall segment still wins over fill
- **WHEN** the operator presses on a wall segment that lies over a floor plate
- **THEN** the wall is selected, not the floor fill
