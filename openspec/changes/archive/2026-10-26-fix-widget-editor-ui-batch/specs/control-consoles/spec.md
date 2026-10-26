# Spec Delta

## MODIFIED Requirements

### Requirement: Composite viewport geometry
When viewport is `composite`, the widget MUST store ordered rows. Each row MUST store height (positive number, unit `cm` or `u`) and ordered cells. Each cell MUST store width (positive number, unit `cm` or `u`), optional contour (default off), `alignH` / `alignV`, and content `empty`, `text`, or `device`. Device cells MUST carry a normal binding; text cells MUST store display text in the cell `text` field. After Save and reload (including reboot), a text cell’s display text MUST equal the text the editor saved — it MUST NOT be replaced by an align token such as `center`, `left`, `right`, `top`, or `bottom`.

#### Scenario: Save composite with mixed cells
- **WHEN** an editor saves a composite widget with one row of a 3 cm cell (text), a 1 u cell (device), and an empty cell with contour on
- **THEN** after reboot those rows, widths, units, content types, and contour flags are unchanged

#### Scenario: Missing composite defaults empty
- **WHEN** a widget has viewport `composite` but no rows
- **THEN** view mode shows an empty composite body until rows are added

#### Scenario: Text cell round-trips typed text
- **WHEN** an editor adds a widget, sets View style to Composite, sets a cell content to Text, types `Hello`, and saves the console
- **THEN** after reopening that console the cell still shows `Hello` and MUST NOT show `center` or another align id in place of the text

## ADDED Requirements

### Requirement: Composite text field parse is not confused by content text
When loading a composite cell whose JSON includes `"content":"text"` and a separate `"text"` string value, the host MUST treat only the `"text"` key’s value as the cell display text. A string value equal to `text` on another key (including `content`) MUST NOT be read as the cell’s display text.

#### Scenario: Content value text does not steal text key
- **WHEN** stored cell JSON has `"content":"text","alignH":"center","text":"Hello"` (any key order with those fields)
- **THEN** after load the cell content is text and the display text is `Hello`
