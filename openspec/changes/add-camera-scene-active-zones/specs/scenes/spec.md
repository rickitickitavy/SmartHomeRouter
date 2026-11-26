## ADDED Requirements

### Requirement: Scene kind Plan or Camera
Each scene MUST have kind `plan` or `camera`. Missing kind on stored scenes MUST behave as `plan`. Camera scenes MUST use a registered camera as background and active zones. Plan scenes MUST keep bake-image and pin authoring. Camera scenes MUST NOT use icon/gauge pins in the operator or simple-user UI.

#### Scenario: Legacy scene is Plan
- **WHEN** a stored scene has no kind field
- **THEN** it behaves as Plan (bake + pins)

#### Scenario: Camera scene has no pins UI
- **WHEN** the operator opens a Camera scene for live view or bind editing
- **THEN** pin placement/controls are not offered; active zones are used instead

### Requirement: Camera scene background refresh
A Camera scene MUST show the selected camera’s JPEG background and MUST refresh it every scene `refreshInterval` seconds. `refreshInterval` MUST be per-scene, default **3**, and MUST clamp to **1..60** seconds when saved/applied.

#### Scenario: Default refresh
- **WHEN** a new Camera scene is created without an explicit refresh interval
- **THEN** the background refreshes every 3 seconds

#### Scenario: Custom refresh
- **WHEN** the operator sets refresh interval to 10 and saves
- **THEN** after reload the Camera scene refreshes every 10 seconds

### Requirement: Active zones
A Camera scene MUST support ordered active zones. Each zone MUST store a polygon (normalized coordinates in the visible camera image box), transparency (0..1), optional ON/OFF colors, a binding that drives ON/OFF state from a device/reportable value, and optional actions for ON and OFF (device or automation, same capability family as console click actions / scene pins). When transparency is less than 1 (not fully transparent), ON and OFF colors MUST be specified. When transparency is 1, colors are not required. Clicking a zone MUST run the action bound to the **current** state (console-equivalent). At design time, zone fill MUST render at **70%** transparency regardless of the stored transparency so zones remain editable. Zone placement and size MUST scale with the scene image box.

#### Scenario: Design-time visibility
- **WHEN** the operator edits zones on a Camera scene
- **THEN** each zone is shown at 70% transparency even if stored transparency is 1

#### Scenario: Runtime colors
- **WHEN** a zone has transparency 0.4, colorOn green, colorOff red, and the bound value is OFF
- **THEN** Place/simple-user show the zone filled red at the configured transparency

#### Scenario: Click current-state action
- **WHEN** the bound state is ON and the operator clicks the zone
- **THEN** the zone’s ON action runs (not the OFF action)

#### Scenario: Fully transparent still clickable
- **WHEN** a zone has transparency 1 and an ON action
- **THEN** the zone has no required fill colors and a click with current state ON still runs the ON action

### Requirement: Zone polygon editing without Shift
Camera-scene zone polygon editing MUST follow the same vertex create/move/select pattern used for floor polygons on the plan editor, except Shift axis-lock MUST NOT be applied.

#### Scenario: No Shift lock
- **WHEN** the operator drags a zone vertex with Shift held
- **THEN** motion is not forced onto a single axis by Shift lock
