## MODIFIED Requirements

### Requirement: Icon scale multiplies viewport fit
When a binding uses view style `icon`, the binding MUST store an optional numeric `iconScale` (default `1` when missing or invalid). In view mode the rendered icon size MUST equal the size produced by the widget viewport fit scale (grid cell, composite cell, or vertical row) **multiplied by** `iconScale`. `iconScale` MUST NOT replace or disable the viewport fit scale. Values less than or equal to zero MUST behave as `1`. Scene pins that size via `sizeW`/`sizeH` outline boxes are exempt from this dialog-driven scale field; they fill their pin box instead.

#### Scenario: Default scale is one
- **WHEN** an icon binding is saved without `iconScale`
- **THEN** rendering behaves as scale `1`

#### Scenario: Scale enlarges icon in grid cell
- **WHEN** an icon binding in a grid cell has `iconScale` `1.5` and the cell fit would make the icon 20px tall
- **THEN** the rendered icon is about 30px tall
