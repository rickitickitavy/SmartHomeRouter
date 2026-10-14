## ADDED Requirements

### Requirement: Binding dialog icon scale
When View style is Icon, the Consoles binding dialog MUST show an Icon scale numeric field (default `1`). Saving MUST store `iconScale` on the binding. The field MUST be hidden when View style is not Icon.

#### Scenario: Scale field appears for icon
- **WHEN** an editor selects View style Icon for an On/Off, binary, or automation binding
- **THEN** the dialog shows Icon scale defaulting to `1`

#### Scenario: Save icon scale
- **WHEN** the editor sets Icon scale to `1.25` and saves
- **THEN** the binding stores `iconScale` `1.25`

### Requirement: Binding dialog show title and title position
The Consoles binding dialog MUST show a Show title checkbox for every device and automation binding (default checked). When Show title is checked, the dialog MUST show a Title position select with Left, Right, Top, and Bottom (default Left). Saving MUST store `showTitle` and `titlePosition`. When Show title is unchecked, Title position MAY stay hidden and the saved `titlePosition` MAY keep its last value or default `left`.

#### Scenario: Show title for automation
- **WHEN** an editor opens an automation binding
- **THEN** Show title is available and checked by default for a new binding

#### Scenario: Title position top
- **WHEN** the editor checks Show title, chooses Top, and saves
- **THEN** the binding stores `showTitle` true and `titlePosition` `top`

### Requirement: Place and user honor icon scale and title layout
The simple-user control page and Place view-mode MUST apply each binding’s `iconScale` as a multiplier on top of viewport fit for icon controls, MUST hide the title when `showTitle` is false, and MUST place the title per `titlePosition` when shown. Edit-mode preview SHOULD reflect the same layout without sending device commands.

#### Scenario: User page hides title
- **WHEN** a simple user opens a console whose binding has `showTitle` false
- **THEN** the binding row shows no title text

#### Scenario: User page title on the right
- **WHEN** a simple user opens a console whose binding has `titlePosition` `right`
- **THEN** the title appears to the right of the control or icon

#### Scenario: Icon larger than cell fit alone
- **WHEN** a simple user opens a grid widget with an icon binding whose `iconScale` is greater than `1`
- **THEN** the icon is larger than the same binding would be at `iconScale` `1` in that cell
