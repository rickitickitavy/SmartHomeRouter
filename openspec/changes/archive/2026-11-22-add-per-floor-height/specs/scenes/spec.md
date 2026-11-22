## ADDED Requirements

### Requirement: Each floor has its own height
Each floor MUST store its own positive height. Default for a new floor MUST be **0.01**. There is no upper bound on floor height (value MUST be greater than zero). Heights MUST persist with the scene. In View/bake, each floor MUST be rendered with that height (prism). All floors MUST share the same base height (MUST NOT be vertically stacked at different start levels).

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
The scene editor MUST provide a separate Floor height spinner immediately after the Add floor button, without a text label (arrows only). The spinner MUST be enabled only when a floor is selected, MUST display that floor’s height, and MUST edit only that floor. When no floor is selected, the spinner MUST be disabled. Wall height MUST remain available as its own control.

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
