## ADDED Requirements

### Requirement: Scene kind and camera selectors
On the Scenes editor, a fixed dropdown for scene kind (**Camera** | **Plan**) MUST appear before the **Edit scene** control. **Edit scene** MUST be visible only when kind is **Plan** (geometry bake editor) and MUST be hidden when kind is **Camera**. When kind is **Camera**, a fixed dropdown of registered cameras MUST appear after the kind control (where Edit scene would be for Plan). Changing kind MUST switch the authoring model (Plan → bake/pins; Camera → camera + zones on the bind preview) without inventing Plan geometry tools on Camera scenes.

#### Scenario: Kind before Edit scene
- **WHEN** the operator views the scene editor chrome with kind Plan
- **THEN** the kind dropdown is shown before Edit scene

#### Scenario: Edit scene hidden for Camera
- **WHEN** kind is Camera
- **THEN** the Edit scene button is not shown

#### Scenario: Camera picker when Camera
- **WHEN** kind is Camera
- **THEN** a camera dropdown is shown after the kind control and lists registered cameras

#### Scenario: No camera picker for Plan
- **WHEN** kind is Plan
- **THEN** the camera dropdown is not shown

### Requirement: Camera scene live view on Place and simple-user
Place and the simple-user page MUST render Camera scenes with the refreshing camera JPEG and active zones (hit-tested polygons). Zone clicks MUST use console-equivalent current-state action handling. Both pages MUST stay behaviorally synchronized for Camera scene display and click.

#### Scenario: Place click
- **WHEN** an operator clicks a Camera-scene zone on Place
- **THEN** the action for the current bound state runs

#### Scenario: Simple-user click
- **WHEN** a simple user clicks the same zone on a Camera scene
- **THEN** the same current-state action path runs as on Place
