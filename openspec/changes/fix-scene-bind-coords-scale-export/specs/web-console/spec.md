## MODIFIED Requirements

### Requirement: Binding dialog Icon scale
When View style is Icon, the Consoles binding dialog MUST show an Icon scale numeric field (default `1`). Saving MUST store `iconScale` on the binding. The field MUST be hidden when View style is not Icon. When the same dialog is opened as a **scene pin** editor, the Icon scale field MUST remain hidden (scene pin size is controlled by outline handles / `sizeW`/`sizeH` only).

#### Scenario: Scale field appears for icon
- **WHEN** the operator sets View style to Icon on a console cell binding
- **THEN** an Icon scale field is visible and editable

#### Scenario: Scale persists
- **WHEN** the operator saves Icon scale `1.25` on a console binding
- **THEN** the binding stores `iconScale` `1.25`

#### Scenario: Scene pin dialog hides Scale
- **WHEN** the operator opens a scene pin binding dialog with View style Icon
- **THEN** Icon scale is not shown
