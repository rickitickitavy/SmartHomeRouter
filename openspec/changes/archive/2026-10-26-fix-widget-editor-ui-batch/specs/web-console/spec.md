# Spec Delta

## MODIFIED Requirements

### Requirement: Console editor sets widget viewport
In the Consoles edit dialog, each widget MUST expose a View style control with options Vertical, Grid, and Composite (default Vertical). Horizontal MUST NOT be offered. When Grid is selected, Columns and Rows MUST appear on one line with View style in the same container, defaulting to 2 and 2. When View style is Vertical or Composite, Columns and Rows MUST be hidden. When Composite is selected, the dialog MUST show composite row/cell authoring. Changing viewport or layout size MUST update the in-dialog preview without persisting until Save.

#### Scenario: Grid fields appear on same line
- **WHEN** the editor sets a widget View style to Grid
- **THEN** Columns and Rows are visible on the same horizontal line as View style

#### Scenario: Vertical hides grid fields
- **WHEN** the editor sets View style to Vertical
- **THEN** Columns and Rows fields are hidden and composite row editors are hidden

#### Scenario: Composite hides Columns and Rows
- **WHEN** the editor sets View style to Composite
- **THEN** Columns and Rows fields are hidden and composite row/cell authoring is shown

### Requirement: Binding dialog content align for all view styles
The Consoles binding dialog MUST show Vertical align for every device and automation binding on a Vertical or Grid widget (default Center) in every binding View style. For a **Vertical** widget, Horizontal align MUST appear when Show title is unchecked and MUST be hidden when Show title is checked so titled rows keep the classic name-left / control-right layout. For a **Grid** widget, Horizontal align MUST remain visible whether Show title is checked or not. Saving MUST store `alignH` and `alignV` for those bindings. When the dialog is opened for a **composite** device cell, Horizontal align and Vertical align MUST NOT appear (the composite cell’s Align H / Align V controls own layout). Saving a composite device cell MUST NOT require binding-level align fields from the dialog; cell `alignH` / `alignV` remain the layout source.

#### Scenario: Horizontal align hidden when title shown on Vertical
- **WHEN** an editor opens a Vertical-widget binding with Show title checked
- **THEN** Horizontal align is hidden and Vertical align remains available

#### Scenario: Horizontal align stays visible when title shown on Grid
- **WHEN** an editor opens a Grid-widget binding with Show title checked
- **THEN** Horizontal align and Vertical align are both visible

#### Scenario: Align visible when title hidden
- **WHEN** an editor unchecks Show title on a Default-view Vertical or Grid binding
- **THEN** Horizontal align and Vertical align are visible

#### Scenario: Save default-view align
- **WHEN** the editor sets Horizontal align to Left and Vertical align to Top on a Default-view Vertical or Grid binding with Show title unchecked and saves
- **THEN** the binding stores `alignH` `left` and `alignV` `top`

#### Scenario: Composite device dialog hides binding align
- **WHEN** the editor opens Add/Edit device for a composite cell that already shows Align H and Align V on the cell chrome
- **THEN** the device dialog does not show Horizontal align or Vertical align

## ADDED Requirements

### Requirement: Enabled widget-editor numeric inputs match dropdown colors
Enabled (not disabled, not readonly) numeric and text inputs in the Consoles widget editor — including Columns, Rows, composite row height, composite cell width, and composite cell text — MUST use the same interactive colors as enabled dropdowns (`--text` on `--bg` or `--surface` with normal `--border`). They MUST NOT use gray / muted / `--chrome-idle` fills that make them look disabled or readonly.

#### Scenario: Columns input looks editable
- **WHEN** the editor sets View style to Grid and looks at the Columns number field beside the View style dropdown
- **THEN** the Columns field uses interactive colors comparable to the View style dropdown, not a grayed readonly look

#### Scenario: Row height input looks editable
- **WHEN** the editor edits a Composite widget and looks at a row height number field
- **THEN** that field uses interactive colors comparable to its unit dropdown, not a grayed readonly look
