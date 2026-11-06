# Spec Delta

## ADDED Requirements

### Requirement: Scenes section for editScenes
The gateway operator console MUST show a main **Scenes** section when the signed-in user has `editScenes` or `isAdmin`. That section MUST list scenes and allow add/edit/delete with the scene designer. Users without those rights MUST NOT see the Scenes section. Simple-user pages MUST NOT expose scene management.

#### Scenario: editScenes sees Scenes
- **WHEN** a user with `editScenes` and not `isAdmin` signs in to the gateway console
- **THEN** the sidebar includes Scenes and can open the designer

#### Scenario: Simple user has no Scenes tab
- **WHEN** a simple user uses the control page
- **THEN** there is no Scenes management tab

### Requirement: Composite cell can choose scene content
In the Consoles composite cell editor, content options MUST include Scene. Choosing Scene MUST offer a picker of persisted scenes. Saving MUST store content `scene` and the selected scene id. Preview MUST show the bake when available (display-only).

#### Scenario: Pick scene for cell
- **WHEN** the editor sets a cell content to Scene and chooses scene `Floor1`
- **THEN** the saved console JSON for that cell has content `scene` and scene id `Floor1`

### Requirement: Place and simple-user render scene cells alike
Place view-mode and the simple-user control page MUST both render composite scene cells with bake, aspect preservation, pin overlays, and the same live icon/gauge interaction. A scene-cell behavior MUST NOT ship on only one of those pages.

#### Scenario: User page matches Place for scene cell
- **WHEN** the same console with a scene cell is opened in Place and on the simple-user page
- **THEN** bake, pin positions, and click behavior match between those surfaces

### Requirement: Account tab full-screen toggle beside Theme
On the signed-in user’s user-named account tab, the Theme row MUST include a full-screen mode toggle on the same line as Theme (and Theme save where present). The control MUST be available to every signed-in user on both the gateway operator console and the simple-user control page. Activating it MUST enter or exit browser full-screen for the console page. Full-screen state MUST NOT be persisted on the user record or restored after reload or later login.

#### Scenario: Operator account tab row
- **WHEN** a signed-in operator opens their user-named account tab
- **THEN** a full-screen toggle appears on the same line as Theme

#### Scenario: Simple user account tab row
- **WHEN** a simple user opens their user-named account tab
- **THEN** a full-screen toggle appears on the same line as Theme

#### Scenario: Full-screen not persisted
- **WHEN** the user enters full-screen then reloads the page
- **THEN** the page is not full-screen and no user theme/API field was written for full-screen

### Requirement: History chart smooth defaults without EMA
On both the operator and simple-user history chart UIs, numeric smooth MUST offer average-window smoothing only. EMA MUST NOT be offered or applied. When the history chart opens, Smooth MUST be unchecked by default and the samples window MUST default to **5**. The same defaults and EMA removal MUST ship on both pages.

#### Scenario: Defaults on open
- **WHEN** a user opens history for a numeric binding on either page
- **THEN** Smooth is unchecked and the samples field shows 5

#### Scenario: No EMA control
- **WHEN** Smooth is enabled on the history chart
- **THEN** there is no Ema/EMA type option and smoothing uses average over the samples window

### Requirement: Security dialog offers editScenes
The user add/edit dialog MUST include an **Edit scenes** checkbox for `editScenes`, alongside Console editor. Persistence and list display MUST include that role.

#### Scenario: Checkbox saves editScenes
- **WHEN** an admin checks Edit scenes for user `sam` and saves
- **THEN** `sam` has `editScenes` and the users table roles text reflects it
