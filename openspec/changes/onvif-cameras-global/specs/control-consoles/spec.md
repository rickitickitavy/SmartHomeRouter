# Spec Delta

## ADDED Requirements

### Requirement: Camera composite cell view modes
A composite camera cell MUST persist camera id, display mode (`jpegRefresh` or `live`), and channel index (1 or 2). In view mode, `jpegRefresh` MUST load the channel snapshot HTTP URL in the browser with periodic refresh; `live` MUST load the channel live HTTP URL in the browser. Missing mode MUST behave as `jpegRefresh`. Missing channel MUST behave as 1. Older consoles without camera cells MUST still load.

#### Scenario: Persist camera cell
- **WHEN** the editor saves a composite camera cell with mode live and channel 2
- **THEN** after reboot that cell still has the same camera id, live mode, and channel 2

#### Scenario: Default mode and channel
- **WHEN** a camera cell is saved with camera id but mode and channel omitted
- **THEN** view mode uses JPEG refresh on channel 1

### Requirement: Camera cell ACL in view mode
View mode MUST render a camera cell only when the signed-in user is allowed to view that camera. When not allowed, the cell MUST show a non-revealing placeholder (no stream or snapshot URL).

#### Scenario: Denied user sees placeholder
- **WHEN** a simple user not listed on the camera opens a console that contains that camera cell
- **THEN** the cell does not expose the camera HTTP URLs

## MODIFIED Requirements

### Requirement: Composite viewport geometry
When viewport is `composite`, the widget MUST store ordered rows. Each row MUST store height (positive number, unit `cm` or `u`) and ordered cells. Each cell MUST store width (positive number, unit `cm` or `u`), optional contour (default off), `alignH` / `alignV`, and content `empty`, `text`, `device`, or `camera`. Device cells MUST carry a normal binding; text cells MUST store display text in the cell `text` field; camera cells MUST store camera id, display mode, and channel. After Save and reload (including reboot), a text cell’s display text MUST equal the text the editor saved — it MUST NOT be replaced by an align token such as `center`, `left`, `right`, `top`, or `bottom`.

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
