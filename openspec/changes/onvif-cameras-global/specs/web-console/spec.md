# Spec Delta

## ADDED Requirements

### Requirement: Cameras section lists registered cameras
The operator console MUST include a main sidebar section **Cameras** visible only to `isAdmin`. The section MUST show a table with columns Active, Name, Type, URL, and Status. The card footer MUST provide Add, Export, and Import. Status MUST follow the onvif-cameras status rules (Offline when inactive; Online or short error when active).

#### Scenario: Open Cameras
- **WHEN** an `isAdmin` selects Cameras
- **THEN** the cameras table is shown and other main panels are hidden

#### Scenario: Non-admin hidden
- **WHEN** a signed-in user without `isAdmin` views the sidebar
- **THEN** Cameras is not shown

### Requirement: Camera dialog fields
Add and row edit MUST open a camera dialog with Name, Active, Type (dropdown; ONVIF only), and ONVIF settings including two channels (snapshot URL and live URL each). Save MUST be the first persist of a new camera. Closing MUST follow the same dirty-check and dismiss rules as other dialogs. Delete MUST ask for confirmation.

#### Scenario: Add then Save
- **WHEN** the operator clicks Add, fills Name and ONVIF URLs, and Saves
- **THEN** the table shows the new camera and a reload still shows it

#### Scenario: Type dropdown
- **WHEN** the operator opens the dialog
- **THEN** Type is a select listing ONVIF and is not a free-text field

### Requirement: Composite cell camera content in editor
Composite cell Content MUST offer Camera in addition to Empty, Text, and Device. Choosing Camera MUST let the operator pick a registered camera, display mode (JPEG with refresh or live video), and channel (1 or 2). Preview MUST NOT fetch through an ESP media proxy.

#### Scenario: Choose camera cell
- **WHEN** the editor sets a composite cell Content to Camera and selects a camera, JPEG refresh, and channel 2
- **THEN** the draft stores that camera, mode, and channel and the preview reflects a camera cell

### Requirement: Camera cell on Place and simple-user
Place view-mode and the simple-user page MUST render camera cells the same way: JPEG refresh uses the channel snapshot URL with periodic reload; live uses the channel live HTTP URL in the browser. Sync rule: behavioral changes MUST land on both pages.

#### Scenario: JPEG refresh on both surfaces
- **WHEN** a console with a JPEG-refresh camera cell is opened on Place and on the simple-user page by an allowed user
- **THEN** both surfaces show the image from the camera snapshot URL with refresh behavior

#### Scenario: Live mode uses live URL
- **WHEN** the cell mode is live video and channel 1 has an HTTP live URL
- **THEN** the browser loads that live URL (not the snapshot URL) for display

### Requirement: Email action camera attachment controls
When an action target is **e-mail**, the Actions row MUST offer a camera-snapshot attach list using the same Add/row/remove controls pattern as email device parameters: each row picks a camera and channel (1 or 2); the operator MAY add multiple rows (at most eight). Switching target away from e-mail MUST hide those controls.

#### Scenario: Multiple attachment rows
- **WHEN** the operator sets an action target to e-mail and adds two camera snapshot rows
- **THEN** both camera/channel rows are visible and editable

## MODIFIED Requirements

### Requirement: Main navigation uses sidebar sections

The console MUST present main sections as a left sidebar strip (horizontal folder strip on a narrow viewport), including Status, WiFi, MQTT, ZigBee, Devices, Automation, Cameras, Security, Consoles, Appearance, and System in the operator-visible set. Selecting a main section MUST show that section’s panel and hide the others without a full page reload. Status MUST show the live summary (counts, packet counters, version) without settings forms. Devices MUST show the registered-device table and add-device flow. Automation MUST show the automations list and edit flow. Cameras MUST show the registered-cameras table and add/edit flow for `isAdmin`.

#### Scenario: Switch main section

- **WHEN** the operator selects WiFi after Status was visible
- **THEN** the WiFi panel is shown and the Status panel is hidden

#### Scenario: Placeholder tabs have no forms

- **WHEN** the operator opens Status
- **THEN** the panel has no configuration controls and does not change device settings

#### Scenario: Open Automation

- **WHEN** an `isAdmin` selects Automation
- **THEN** the Automation panel is shown and other main panels are hidden

#### Scenario: Open Cameras

- **WHEN** an `isAdmin` selects Cameras
- **THEN** the Cameras panel is shown and other main panels are hidden

### Requirement: Console editor composite cell authoring
When Viewport is Composite, the editor MUST provide Add row. Each row MUST show a height configurer on the left (value plus cm or units). Each cell MUST show a width configurer above it (value plus cm or units), stacked horizontally across the row. The editor MUST let the operator set cell content to text, device (binding dialog), camera (camera/mode/channel), or empty, and MUST offer an optional cell contour control. The editor MUST support drag-and-drop of cells within a row and across rows of the same widget; after a drop it MUST recalculate composite row heights with the existing size rules and MUST remove an emptied source row. Preview MUST reflect sizes and content without sending device commands.

#### Scenario: Add row
- **WHEN** the editor clicks Add row on a composite widget
- **THEN** a new row appears with at least one cell and left height configurer

#### Scenario: Configure cell width units
- **WHEN** the editor sets a cell width to 2 units
- **THEN** the draft stores width 2 with unit `u` and the preview updates

#### Scenario: Empty cell with contour
- **WHEN** the editor sets a cell to empty and enables contour
- **THEN** preview shows an outlined empty cell and save persists those settings

#### Scenario: Drag cell across rows
- **WHEN** the editor drags a cell from one composite row onto another row and drops it
- **THEN** the draft moves that cell, refreshes row heights, and removes the source row if it has no cells left

#### Scenario: Camera cell content
- **WHEN** the editor sets a cell to camera with a registered camera and channel 1
- **THEN** preview shows a camera cell and save persists camera id, mode, and channel

### Requirement: Email action editor in automations
When an action target is **e-mail**, the Actions row MUST show receivers (text; semicolon-separated), theme (subject), message (multiline body), a parameter list with Add/Remove for at most eight rows, and optional camera snapshot attachment (camera + channel). Each parameter row MUST let the operator pick device, attribute (same known-parameter-or-CUSTOM pattern as Trigger for reading status), and title. The row MUST NOT collect a static value field for the parameter. Switching target away from e-mail MUST hide email-specific fields.

#### Scenario: Email action fields visible
- **WHEN** the operator sets an action target to e-mail
- **THEN** receivers, theme, message, and the parameter list controls are visible

#### Scenario: Parameter uses device attribute title
- **WHEN** the operator adds an email parameter row
- **THEN** the row offers device, attribute, and title inputs and does not offer a static value input

#### Scenario: Eighth parameter is the last Add
- **WHEN** the email action already has eight parameters
- **THEN** Add is disabled or does not create a ninth row

#### Scenario: Optional camera attach
- **WHEN** the operator enables camera attach on an e-mail action and picks channel 2
- **THEN** save persists the camera id and channel with that action
