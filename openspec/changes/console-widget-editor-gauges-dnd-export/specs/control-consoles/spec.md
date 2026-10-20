# Spec Delta

## MODIFIED Requirements

### Requirement: Arc gauge value font size
The arc gauge center value text MUST scale proportionally with the rendered arc size so the digit-to-arc ratio stays visually consistent across small and large cells (grid or composite). A small minimum and maximum font size MAY clamp the result so tiny phone cells stay readable and very large cells do not dominate. The same layout MUST produce the same digit size on Place and on the simple-user page. Linear gauge value text size is unchanged by this requirement.

#### Scenario: Larger cell larger digits
- **WHEN** two arc gauges on the same console use the same relative layout but one cell is substantially larger than the other
- **THEN** the larger gauge’s center digits are larger in proportion to its arc (unless both are already at the same clamp)

#### Scenario: Tiny cell stays readable
- **WHEN** an arc gauge is shown in a very small composite or grid cell on a phone-width viewport
- **THEN** the center digits are at least the configured minimum readable size

#### Scenario: Place matches simple-user digit size
- **WHEN** the same console layout with arc gauges is opened in Place and on the simple-user page at the same viewport size
- **THEN** corresponding gauges show the same center digit font size

### Requirement: Binding content horizontal and vertical align
Every device and automation binding MUST support `alignH` (`left`, `center`, or `right`, default `center` when missing or invalid) and `alignV` (`top`, `center`, or `bottom`, default `center` when missing or invalid) for every view style including `default`. View mode MUST place the binding content in its viewport cell or slot according to those values when the title is hidden. When `showTitle` is true with title position left or right, view mode MUST keep the classic name-and-control single-row layout. When `alignH` is missing but legacy `iconAlign` is present, `alignH` MUST take that value. For view style `icon` with `alignH` `center` (or composite cell `alignH` `center` when that cell align applies), the icon glyph MUST appear visually centered in the cell, not optically shifted toward the right. Spacer bindings keep their existing align fields and are unchanged by this requirement.

#### Scenario: Default view honors horizontal align
- **WHEN** an On/Off binding with view style `default` is saved with `showTitle` false and `alignH` `right`
- **THEN** after reload view mode aligns that binding’s content to the right in its cell or slot

#### Scenario: Default view honors vertical align
- **WHEN** a device binding with view style `default` is saved with `alignV` `bottom` in a grid cell
- **THEN** after reload view mode places that binding’s content toward the bottom of the cell

#### Scenario: Legacy iconAlign maps to alignH
- **WHEN** a stored icon binding has `iconAlign` `left` and no `alignH`
- **THEN** after load view mode treats horizontal align as `left`

#### Scenario: Centered icon is not right-biased
- **WHEN** a composite cell shows an icon binding (for example `mdi:engine`) with horizontal align `center` and no title
- **THEN** equal empty space appears on the left and right of the icon inside the cell

### Requirement: Save, export, and empty-only import
The editor MUST persist the console only when Save is used. Export MUST download that console as JSON. The download filename MUST be derived from the console name: Latin letters, digits, `.`, `-`, and `_` MAY be kept; non-Latin letters MUST be transliterated to Latin (for example Cyrillic `Котел` → a Latin slug such as `Kotel`); other unsafe filename characters MUST become `_`. Import from JSON MUST be enabled only when the console under edit has no widgets and no device bindings. Import MUST replace the empty layout from a valid export file.

#### Scenario: Export then import empty
- **WHEN** the editor exports console A and later imports that JSON into a new empty console
- **THEN** the new console matches A’s widgets and bindings

#### Scenario: Import disabled when not empty
- **WHEN** the console under edit already has a widget
- **THEN** Import from JSON is not available

#### Scenario: Cyrillic console name becomes translit filename
- **WHEN** the editor exports a console named `Котел`
- **THEN** the download filename is a `.json` name based on a Latin transliteration of `Котел` (not only underscores)

## ADDED Requirements

### Requirement: Composite cell drag reorder across rows
In composite edit mode the editor MUST let the operator drag a cell and drop it onto another position in the same row or in a different row of the same widget. After the drop, the editor MUST recalculate that widget’s composite row heights using the existing composite size rules and MUST remove a source row that became empty.

#### Scenario: Move cell to another row
- **WHEN** the editor drags a cell from row 1 onto row 2 and drops it
- **THEN** the draft places that cell in row 2 and row heights refresh for the affected rows

#### Scenario: Empty source row removed
- **WHEN** the editor moves the last cell out of a row
- **THEN** that empty row is removed from the draft
