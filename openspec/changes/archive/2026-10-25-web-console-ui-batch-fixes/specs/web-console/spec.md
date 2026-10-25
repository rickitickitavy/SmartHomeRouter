# Spec Delta

## ADDED Requirements

### Requirement: Gauge color picker includes Cyan
The Consoles binding dialog gauge color picker MUST include a Cyan preset at RGB `(59, 130, 246)` (`#3b82f6`) in addition to the existing presets.

#### Scenario: Cyan preset available
- **WHEN** the editor opens the gauge color picker presets
- **THEN** Cyan at `#3b82f6` is available

### Requirement: Grid Columns and Rows share View style row
When View style is Grid, Columns and Rows (with labels) MUST appear on one horizontal line with the View style dropdown, in the same container. When View style is not Grid, Columns and Rows MUST stay hidden.

#### Scenario: Same line when Grid
- **WHEN** the editor sets View style to Grid
- **THEN** View style, Columns, and Rows are on one horizontal line in the same container

### Requirement: Composite Remove row uses recycle bin
Composite row remove MUST use the recycle-bin icon button (not the text “Remove row”), consistent with other delete controls.

#### Scenario: Recycle bin remove row
- **WHEN** the editor views a composite row’s tools
- **THEN** remove is a recycle-bin icon button

### Requirement: Add cell label
The widget editor control that adds a device/automation binding MUST be labeled **Add cell** (not Add binding).

#### Scenario: Add cell wording
- **WHEN** the editor views a vertical or grid widget’s add actions
- **THEN** the primary add control reads Add cell

### Requirement: App shell has no top or bottom viewport margin
The global app viewport chrome MUST NOT add top or bottom margin/padding outside the content; left/right padding MAY remain.

#### Scenario: Flush top and bottom
- **WHEN** the signed-in gateway console is shown
- **THEN** there is no extra top or bottom margin between the viewport edges and the app shell content

### Requirement: Account tab replaces Appearance and Sign out
The main sidebar MUST NOT include Appearance or a standalone Sign out control. It MUST include a tab labeled with the signed-in user name whose panel contains theme controls and Sign out. Simple users MUST get an equivalent trailing tab with the same theme and Sign out content. Theme persistence remains per signed-in user via the existing theme API. The login form MUST stay dark.

#### Scenario: Gateway account tab
- **WHEN** an operator with a gateway sidebar is signed in as alice
- **THEN** the sidebar shows a tab labeled alice with theme and Sign out, and Appearance is absent

#### Scenario: Simple user account tab
- **WHEN** a simple user is signed in
- **THEN** a trailing tab with their user name offers theme and Sign out

## MODIFIED Requirements

### Requirement: Gauge range color picker presets
The Consoles binding dialog gauge color picker MUST offer preset swatches the operator can apply in one click, including white, gray, light gray, red, green, blue, yellow, orange, violet, and cyan (`#3b82f6`). Choosing a preset MUST set the active color for adding or editing a range. A free-form color control MAY remain available beside the presets.

#### Scenario: Preset fills range color
- **WHEN** the editor opens Gauge ranges and clicks the cyan preset
- **THEN** the active range color becomes `#3b82f6`

#### Scenario: Cyan among presets
- **WHEN** the editor views the gauge color picker presets
- **THEN** cyan is available along with the other listed presets

### Requirement: Appearance follows Consoles in the sidebar
The main sidebar MUST NOT include an Appearance section. Automation MUST sit immediately after Devices. Relative order MUST be Devices, Automation, Security, Consoles, then the account (user-name) tab and System (other earlier tabs unchanged).

#### Scenario: Admin sidebar order without Appearance
- **WHEN** an `isAdmin` views the gateway sidebar
- **THEN** Automation is after Devices, Consoles has no Appearance after it, and the user-name account tab is present

### Requirement: Appearance theme is the signed-in user
Theme picker and Save MUST live on the signed-in user’s account tab (not Appearance). After login the console MUST apply that user’s saved theme. Saving Theme there MUST persist that theme on the signed-in user only. The login form MUST stay dark regardless of any user theme. A later login of the same user MUST reopen in that user’s last saved theme.

#### Scenario: User theme after login
- **WHEN** user `alice` has theme `light` and signs in from the dark login form
- **THEN** the console chrome switches to Light

#### Scenario: Save theme is per user
- **WHEN** `alice` saves Dark and `bob` has Light saved
- **THEN** `alice` later logs in to Dark and `bob` later logs in to Light

### Requirement: Console editor sets widget viewport
In the Consoles edit dialog, each widget MUST expose a View style control with options Vertical, Grid, and Composite (default Vertical). Horizontal MUST NOT be offered. When Grid is selected, Columns and Rows MUST appear on one line with View style in the same container, defaulting to 2 and 2. When Composite is selected, the dialog MUST show composite row/cell authoring. Changing viewport or layout size MUST update the in-dialog preview without persisting until Save.

#### Scenario: Grid fields appear on same line
- **WHEN** the editor sets a widget View style to Grid
- **THEN** Columns and Rows are visible on the same horizontal line as View style

#### Scenario: Vertical hides grid fields
- **WHEN** the editor sets View style to Vertical
- **THEN** Columns and Rows fields are hidden and composite row editors are hidden
