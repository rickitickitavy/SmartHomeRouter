## ADDED Requirements

### Requirement: Binding icon scale multiplies viewport fit
When a binding uses view style `icon`, the binding MUST store an optional numeric `iconScale` (default `1` when missing or invalid). In view mode the rendered icon size MUST equal the size produced by the widget viewport fit scale (grid cell, horizontal slot, or vertical row) **multiplied by** `iconScale`. `iconScale` MUST NOT replace or disable the viewport fit scale. Values less than or equal to zero MUST behave as `1`.

#### Scenario: Default icon scale is one
- **WHEN** an icon binding is saved without `iconScale`
- **THEN** after reload the icon uses only the viewport fit size (effective multiplier `1`)

#### Scenario: Icon scale multiplies grid fit
- **WHEN** an icon binding in a grid cell has `iconScale` `1.5` and the cell fit would make the icon 20px tall
- **THEN** the icon renders at 30px tall (1.5 × fit), not 1.5× some fixed base that ignores the cell

### Requirement: Binding show title and title position
Every device and automation binding MUST support `showTitle` (boolean, default `true` when missing) and `titlePosition` (`left`, `right`, `top`, or `bottom`, default `left` when missing or invalid). When `showTitle` is `false`, view mode MUST NOT show the binding title text. When `showTitle` is `true`, view mode MUST place the title relative to the binding controls according to `titlePosition`. Spacer bindings MUST ignore these fields.

#### Scenario: Hide title
- **WHEN** a binding is saved with `showTitle` false
- **THEN** after reload view mode shows the control or icon without a title label

#### Scenario: Title below control
- **WHEN** a binding is saved with `showTitle` true and `titlePosition` `bottom`
- **THEN** view mode places the title under the control or icon

#### Scenario: Defaults match today’s layout
- **WHEN** a binding has no `showTitle` or `titlePosition` fields
- **THEN** view mode shows the title to the left of the controls as today

### Requirement: Icon scale and title fields persist
The host MUST persist `iconScale`, `showTitle`, and `titlePosition` on bindings through save and reboot. Automation bindings MUST persist `showTitle` and `titlePosition` the same way as device bindings. `iconScale` MUST persist for icon-styled bindings (binary, On/Off, or automation with view style `icon`).

#### Scenario: Round-trip after reboot
- **WHEN** an editor saves an On/Off icon binding with `iconScale` `2`, `showTitle` false, and later an automation binding with `titlePosition` `top`
- **THEN** after reboot those field values are unchanged
