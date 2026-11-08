## MODIFIED Requirements

### Requirement: Scene geometry export and import
Export (full scene = house scheme plus pin bindings), Export scheme (house geometry and camera: floors, walls, windows, doors — no pins), and Import MUST be available on the **scene bind editor** (the dialog where the operator manages the bake image and pins), not on the plan geometry editor. The plan geometry editor MUST keep Save/Cancel for geometry only. Import MUST remain enabled only when the imported target model is empty as previously defined, MUST auto-detect full vs scheme JSON, MUST restore pins when present, and MUST regenerate the bake after import.

#### Scenario: Export lives on bind editor
- **WHEN** the operator opens the scene dialog used to place pins on the bake
- **THEN** Export, Export scheme, and Import controls are present there

#### Scenario: Geometry editor has no export/import
- **WHEN** the operator opens the plan geometry editor
- **THEN** Export / Export scheme / Import are not offered on that dialog

#### Scenario: Scheme keeps walls
- **WHEN** the operator exports scheme from a scene that has walls and windows
- **THEN** the scheme JSON includes those walls and windows

## ADDED Requirements

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
