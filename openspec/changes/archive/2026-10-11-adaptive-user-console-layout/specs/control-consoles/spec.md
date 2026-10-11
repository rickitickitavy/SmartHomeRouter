# Spec Delta

## ADDED Requirements

### Requirement: View-mode console fills the viewport
On the simple-user control page, the signed-in console shell MUST occupy the full viewport width and height. Content that does not fit MUST scroll inside the console panels area; the page MUST NOT leave unused horizontal margins from a max-width cap on that shell.

#### Scenario: Full-width shell on a phone
- **WHEN** a simple user is signed in on a viewport narrower than 13 cm
- **THEN** the control console shell spans the full viewport width

#### Scenario: Full-height shell
- **WHEN** a simple user is signed in
- **THEN** the control console shell spans the full viewport height and console tabs plus sign-out remain reachable without leaving the viewport chrome

### Requirement: Narrow console stacks widgets in one column
In view mode, when the visible console content width is less than 13 cm, visible widgets MUST stack in a single column at 100% of that width. Widgets that are hidden (including spacer placeholders that are not shown) MUST NOT occupy a grid cell and MUST NOT affect column placement of visible widgets. Typography and primary controls in that view MUST remain sized for comfortable reading and touch at that width.

#### Scenario: Phone stacks three widgets
- **WHEN** view mode shows a console whose content width is under 13 cm and three widgets are visible
- **THEN** those three widgets appear one under another, each using the full console content width

#### Scenario: Hidden widgets ignored
- **WHEN** view mode has two visible widgets and one hidden spacer between them on a console narrower than 13 cm
- **THEN** the two visible widgets stack with no empty cell reserved for the spacer

### Requirement: Wide console uses two capped columns
In view mode, when the visible console content width is 13 cm or greater, visible widgets MUST fill a two-column grid left-to-right then top-to-bottom. Each widget MUST take half of the row width and MUST NOT exceed 10 cm width. Saved console `columns` MUST NOT force more than two columns in this view mode. Hidden widgets MUST NOT take a cell.

#### Scenario: Two-column fill order
- **WHEN** view mode shows four visible widgets on a console at least 13 cm wide
- **THEN** widget order is top-left, top-right, bottom-left, bottom-right

#### Scenario: Widget max width
- **WHEN** view mode shows widgets on a console wider than 20 cm
- **THEN** each widget column is at most 10 cm wide

#### Scenario: Saved many columns still two in view
- **WHEN** a console is persisted with `columns` greater than 2 and a simple user opens it in view mode at least 13 cm wide
- **THEN** visible widgets still lay out in at most two columns under the width rules above
