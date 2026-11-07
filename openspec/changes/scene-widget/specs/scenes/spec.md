# Spec Delta

## Purpose

Defines first-class Scene entities: schematic multi-floor plans with baked Full HD imagery, device/automation pins, a Scenes management tab, and `editScenes` authorization for create/edit/delete.

## ADDED Requirements

### Requirement: Scenes persist as first-class entities
The host MUST persist scenes with a unique id, name, editable scene model (geometry, colors, scene size, multi-floor heights, camera yaw/pitch), bake reference for a ≥ Full HD image, and an ordered list of pins. Each pin MUST target a device or automation, use relative coordinates in 0..1, and use Icon or Gauge styles per common console eligibility (no pin titles). After reboot the same scenes MUST load.

#### Scenario: Save and reload scene with pin
- **WHEN** an editor saves scene `Floor1` with a bake and a device pin at (0.4, 0.6)
- **THEN** after reboot that scene still has the model, bake, and pin at (0.4, 0.6)

### Requirement: Scene designer and bake
The Scenes editor MUST provide top-view authorship for floors, walls, windows, and doors with the geometry rules previously defined for this change (defaults: sceneSize 1.0, wall height 0.15, wall length sceneSize/7; multi-floor; no furniture props). View scene MUST toggle 3D-style preview with camera orbit about the scene center. Save MUST commit the model and regenerate the bake; Cancel MUST discard editor changes.

#### Scenario: Save regenerates bake
- **WHEN** the editor changes walls under View scene angles and activates Save
- **THEN** the stored model and bake update and Cancel is not required to keep those changes

#### Scenario: Cancel keeps prior bake
- **WHEN** the editor changes geometry and activates Cancel
- **THEN** the previously committed model and bake remain unchanged

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
