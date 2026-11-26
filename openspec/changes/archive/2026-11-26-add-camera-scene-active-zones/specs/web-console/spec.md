## ADDED Requirements

### Requirement: Scene kind and camera selectors
On the Scenes editor, a fixed dropdown for scene kind (**Camera** | **Plan**) MUST appear before the **Edit scene** control. When kind is **Camera**, a fixed dropdown of registered cameras MUST appear after **Edit scene**. Changing kind MUST switch the authoring model (Plan → bake/pins; Camera → camera + zones) without inventing Plan geometry tools on Camera scenes.

#### Scenario: Kind before Edit scene
- **WHEN** the operator views the scene editor chrome
- **THEN** the kind dropdown is shown before Edit scene

#### Scenario: Camera picker when Camera
- **WHEN** kind is Camera
- **THEN** a camera dropdown is shown after Edit scene and lists registered cameras

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
