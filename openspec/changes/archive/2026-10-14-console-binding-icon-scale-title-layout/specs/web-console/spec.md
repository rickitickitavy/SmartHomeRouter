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

### Requirement: Binding dialog content align for all view styles
The Consoles binding dialog MUST show Vertical align for every device and automation binding (default Center) in every View style. Horizontal align MUST appear when Show title is unchecked and MUST be hidden when Show title is checked so titled rows keep the classic name-left / control-right layout. Saving MUST store `alignH` and `alignV`.

#### Scenario: Horizontal align hidden when title shown
- **WHEN** an editor opens a binding with Show title checked
- **THEN** Horizontal align is hidden and Vertical align remains available

#### Scenario: Align visible when title hidden
- **WHEN** an editor unchecks Show title on a Default-view binding
- **THEN** Horizontal align and Vertical align are visible

#### Scenario: Save default-view align
- **WHEN** the editor sets Horizontal align to Left and Vertical align to Top on a Default-view binding with Show title unchecked and saves
- **THEN** the binding stores `alignH` `left` and `alignV` `top`

### Requirement: Place and user honor content align
The simple-user control page and Place view-mode MUST apply each binding’s `alignV` for Default, Icon, and Gauge view styles. When `showTitle` is false, they MUST also apply `alignH`. When `showTitle` is true with title position left or right, the row MUST keep name and control on one line (control opposite the title). Edit-mode preview SHOULD reflect the same layout without sending device commands. When Show title is unchecked in edit mode, the plate MUST show a muted registered device name (not the binding Title and not the IEEE).

#### Scenario: User page default view right align without title
- **WHEN** a simple user opens a console whose Default-view binding has `showTitle` false and `alignH` `right`
- **THEN** the binding content is right-aligned in its cell or slot

#### Scenario: Titled row keeps control on the same line
- **WHEN** a simple user opens a console whose binding has `showTitle` true and `titlePosition` `left`
- **THEN** the title and switch or icon appear on one row

#### Scenario: Edit preview shows muted device name
- **WHEN** an editor views a binding with `showTitle` false in the Consoles edit dialog
- **THEN** the plate shows the registered device name in muted text

### Requirement: Widget header to content gap
Console widgets on Place and the simple-user page MUST leave a **5mm** gap between the widget header (title / group On/Off) and the binding content list.

#### Scenario: Five millimetre header gap
- **WHEN** a simple user opens a console widget that has a title and at least one binding
- **THEN** the space between the widget title row and the first binding content is 5mm
