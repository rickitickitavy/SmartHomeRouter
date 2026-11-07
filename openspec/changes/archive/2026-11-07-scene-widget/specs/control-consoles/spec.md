# Spec Delta

## MODIFIED Requirements

### Requirement: Composite viewport geometry
When viewport is `composite`, the widget MUST store ordered rows. Each row MUST store height (positive number, unit `cm` or `u`) and ordered cells. Each cell MUST store width (positive number, unit `cm` or `u`), optional contour (default off), `alignH` / `alignV`, and content `empty`, `text`, `device`, `camera`, or `scene`. Device cells MUST carry a normal binding; text cells MUST store display text in the cell `text` field; camera cells MUST store camera id, display mode, and channel; scene cells MUST store a scene id referencing a persisted scene. After Save and reload (including reboot), a text cell’s display text MUST equal the text the editor saved — it MUST NOT be replaced by an align token such as `center`, `left`, `right`, `top`, or `bottom`.

#### Scenario: Save composite with mixed cells
- **WHEN** an editor saves a composite widget with one row of a 3 cm cell (text), a 1 u cell (device), and an empty cell with contour on
- **THEN** after reboot those rows, widths, units, content types, and contour flags are unchanged

#### Scenario: Missing composite defaults empty
- **WHEN** a widget has viewport `composite` but no rows
- **THEN** view mode shows an empty composite body until rows are added

#### Scenario: Text cell round-trips typed text
- **WHEN** an editor adds a widget, sets View style to Composite, sets a cell content to Text, types `Hello`, and saves the console
- **THEN** after reopening that console the cell still shows `Hello` and MUST NOT show `center` or another align id in place of the text

#### Scenario: Camera cell round-trips
- **WHEN** an editor saves a composite cell with content camera, a registered camera id, mode jpegRefresh, and channel 1
- **THEN** after reboot that cell still has content camera with the same id, mode, and channel

#### Scenario: Scene cell round-trips
- **WHEN** an editor saves a composite cell with content scene and scene id `Floor1`
- **THEN** after reboot that cell still has content scene and scene id `Floor1`

## ADDED Requirements

### Requirement: Scene cell shows bake and pins
In view mode, a composite scene cell MUST render that scene’s baked image with aspect ratio preserved inside the cell and MUST overlay the scene’s pins at their relative coordinates with Icon/Gauge behavior matching other console bindings. Missing or deleted scene ids MUST show a non-revealing empty placeholder.

#### Scenario: Scene cell overlays pins
- **WHEN** a simple user opens a console whose composite cell references a scene that has an icon pin at (0.5, 0.5)
- **THEN** the bake fills the cell without distortion and the pin appears at the cell center and is clickable
