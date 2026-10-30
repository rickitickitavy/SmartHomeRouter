# control-consoles Specification

## Purpose

Host-persisted control consoles that household users open after login: tabbed layouts of widgets and bound devices with live on/off, covering, and sensor controls.

## Requirements

### Requirement: Consoles persist with widgets and devices
The host MUST persist control consoles. Each console MUST have a unique id, a name, an active flag, and an ordered list of widgets. Each widget MUST have a title, a show-group-on-off flag, a viewport (`vertical`, `grid`, or `composite`), optional grid Columns and Rows when viewport is `grid`, optional composite layout when viewport is `composite`, a `fullWidth` flag (default false), and an ordered list of bindings (for `vertical` and `grid`, or as device-cell content for `composite`). A binding MUST target either a registered device (optional channel, title, optional unit, optional param/custom cluster/attrId, optional view style and gauge config) or an automation by `name` (optional row title). Empty consoles MUST be allowed. After reboot the same consoles MUST load. The host MUST allow at most 32 consoles. Device-only exports from older firmware MUST still load. Missing viewport MUST load as `vertical`; missing view style as `default`; missing `fullWidth` as false. Viewport value `horizontal` on load MUST behave as `vertical`. Spacer widgets MUST NOT be retained after load or save.

#### Scenario: Save and reload with automation binding
- **WHEN** an editor saves console `Kitchen` with one widget that binds automation `yard-light`
- **THEN** after reboot `Kitchen` still has that widget and automation binding

#### Scenario: Thirty-third console rejected
- **WHEN** the host already has 32 consoles and an editor saves a new one
- **THEN** the host does not persist the thirty-third console and returns an error

#### Scenario: Save and reload grid viewport and gauge
- **WHEN** an editor saves a widget with viewport `grid`, Columns 2, Rows 2, and a gauge binding
- **THEN** after reboot that widget still has those viewport and gauge settings

#### Scenario: Legacy horizontal becomes vertical
- **WHEN** stored JSON has a widget with viewport `horizontal`
- **THEN** after load that widget behaves as viewport `vertical`

#### Scenario: Spacer widgets dropped
- **WHEN** stored JSON includes a spacer widget between two real widgets
- **THEN** after load and subsequent save only the two real widgets remain in order

#### Scenario: Full width persists
- **WHEN** an editor saves a widget with `fullWidth` true
- **THEN** after reboot that widget still has `fullWidth` true

### Requirement: Widget viewport layout
Each widget MUST have a viewport of `vertical` (default), `grid`, or `composite`. Missing viewport on load MUST behave as `vertical`. Viewport `horizontal` on load MUST behave as `vertical`. When viewport is `grid`, the widget MUST store positive integer Columns and Rows (defaults **2** and **2** when missing or invalid). In view mode, bindings MUST lay out according to viewport: vertical stacks top-to-bottom; grid fills cells left-to-right then top-to-bottom for Columns × Rows; composite lays out per the composite geometry requirement. If there are more bindings than Columns × Rows on a grid, remaining bindings MUST continue in additional rows using the same column count. Controls and gauges inside a grid cell, composite cell, or vertical row MUST scale to fit that slot. Binding view styles `default`, `gauge`, and `icon` MUST keep their existing behaviors inside those slots.

#### Scenario: Default viewport is vertical
- **WHEN** a saved widget has no viewport field
- **THEN** view mode stacks bindings vertically as today

#### Scenario: Grid fill order
- **WHEN** a widget viewport is `grid` with Columns 2 and Rows 2 and four numeric bindings
- **THEN** view mode shows them in a 2×2 grid filled left-to-right, top-to-bottom

#### Scenario: Grid overflow adds rows
- **WHEN** a widget viewport is `grid` with Columns 2 and Rows 2 and five bindings
- **THEN** the fifth binding appears on a third row in the first column

#### Scenario: Horizontal row
- **WHEN** stored JSON has a widget with viewport `horizontal` and three bindings
- **THEN** view mode stacks those three bindings vertically (legacy horizontal is treated as vertical)

### Requirement: Device binding parameter selection
A device binding (not an automation binding) MUST allow selecting the displayed or commanded parameter using the same fields and dependent behavior as automation “device to trigger”: registered device, channel when multi-channel, param from the device’s parameter list, and when param is `custom`, cluster and attrId (hex). For a device with type `userData`, the param list MUST be the logical names from that device’s `userDataFields` (schema order) plus `custom`. Missing param on load MUST default to the current channel/status behavior used today for that device class; for `userData` with a non-empty schema and missing param, the host MAY default to the first field name. Automation bindings MUST NOT require these parameter fields.

#### Scenario: Custom attribute binding
- **WHEN** the editor saves a device binding with param `custom`, cluster, and attrId
- **THEN** after reload that binding still has those values and view mode shows that attribute’s value when available

#### Scenario: Default param omitted
- **WHEN** an older console binding has ieee and channel but no param
- **THEN** the host still loads it and view mode behaves as before this change

#### Scenario: User data field dropdown
- **WHEN** the editor binds a `userData` device whose schema includes `temp` and `label`
- **THEN** the param control lists those names and Save can store param `temp`

### Requirement: Numeric binding view style and gauge
For a device binding whose selected parameter is numeric, the binding MUST support view style `default` (current text/control presentation) or `gauge`. Numeric parameters MUST include: `sensorValue` for Measurement-class device types (at least temperature, pressure, humidity, illuminance, flow, metering, and electrical measurement), `battery`, `position`, `lightLevel`, and `custom` when the operator intends a numeric attribute. Missing view style MUST behave as `default`. When view style is `gauge`, the binding MUST store gauge type `arc` (default) or `linear`, numeric min and max, and an ordered list of color ranges. Each range MUST have a top limit and a color; the bottom of the first range MUST be the gauge min; each later range’s bottom MUST be the previous range’s top. When gauge type is `linear`, the binding MUST also store one linear style among `bar`, `filled`, `vertical`, and `verticalFilled` (missing or invalid MUST behave as `bar`). Arc gauges MUST ignore linear style. Non-numeric bindings (On/Off status, covering move status, automation) MUST ignore gauge settings and keep their existing controls.

#### Scenario: Temperature sensor value allows gauge
- **WHEN** an editor saves a temperature Measurement binding with param `sensorValue`, view style `gauge`, type `arc`, min 0, max 40, and one range top 25 with a color
- **THEN** after reboot that binding still has those gauge fields

#### Scenario: Missing view style defaults to default
- **WHEN** a numeric binding has no viewStyle field
- **THEN** view mode uses the existing text or control presentation

#### Scenario: On/Off ignores gauge
- **WHEN** an On/Off binding has view style fields set in stored JSON
- **THEN** view mode shows the On/Off switch even if gauge fields are present in stored JSON

#### Scenario: Linear style persists
- **WHEN** an editor saves a linear gauge binding with linear style `verticalFilled`
- **THEN** after reboot that binding still has gauge type `linear` and linear style `verticalFilled`

#### Scenario: Missing linear style defaults to bar
- **WHEN** a stored linear gauge has no linear style field
- **THEN** view mode draws the existing horizontal growing-fill bar

### Requirement: Gauge appearance and scaling
In view mode, a gauge binding MUST render a Home Assistant–like arc or linear gauge using the configured min, max, value, color ranges, and (for linear) linear style. The gauge MUST scale to fit its viewport cell (grid or composite) or vertical row without overflowing the widget content box. Values outside min/max MUST clamp to the scale ends for drawing; the numeric readout MAY still show the raw value. Linear styles `filled` and `verticalFilled` MUST draw a full range-colored band (same range colors as the arc) as a straight track with a small value arrow, not the growing single-color fill bar. Styles `vertical` and `verticalFilled` MUST orient the track vertically and MUST use the maximum available height of the binding’s container after reserving space for the binding label (title) placement.

#### Scenario: Arc fits grid cell
- **WHEN** a grid viewport cell shows an arc gauge
- **THEN** the gauge fits inside that cell without overlapping neighboring cells

#### Scenario: Linear fits horizontal slot
- **WHEN** a composite viewport cell shows a linear gauge binding with style `bar` or `filled`
- **THEN** the bar scales to the cell width and stays within the cell

#### Scenario: Filled linear matches arc coloring with arrow
- **WHEN** a linear gauge uses style `filled` with the same ranges as an arc gauge
- **THEN** the track shows the full range-colored band straightened and a small arrow at the live value

#### Scenario: Vertical linear uses max height minus label
- **WHEN** a linear gauge uses style `vertical` or `verticalFilled` in a cell or row that also shows a binding title
- **THEN** the track uses the maximum remaining container height after the title’s space and does not overflow the container

### Requirement: Gauge value uses measurement engineering units
When a Measurement-class binding is rendered as a gauge, the gauge needle and numeric scale MUST use the same engineering-unit value that the default measurement text display would use for that binding (including the binding’s selected unit when applicable). The gauge MUST NOT treat Measurement-class readings as non-numeric solely because the live state string includes units or formatting. The visible gauge value text MUST be the number only and MUST NOT append a measure unit suffix (for example °C, °F, K, hPa, %, lx).

#### Scenario: Temperature gauge tracks Celsius reading
- **WHEN** a temperature Measurement binding with view style `gauge` and unit Celsius shows live state that formats as a temperature reading
- **THEN** the gauge position reflects that Celsius number within the configured min and max

#### Scenario: Gauge readout omits units
- **WHEN** a temperature Measurement gauge shows a live reading of 22.5 °C in default text style terms
- **THEN** the gauge value text is `22.5` with no unit suffix

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

### Requirement: Binary sensor icon view style
For Binary Input / Binary Output / Binary Value (and BACnet binary) device bindings, On/Off device bindings, automation bindings, and `userData` bindings whose selected schema field type is `bool`, the binding MUST support view style `default` or `icon`. When view style is `icon`, the binding MUST store an icon id from the Material Design Icons set used by Home Assistant (`mdi:…`). Missing view style MUST behave as `default`. Binary sensor types and `userData` bool fields MUST NOT offer gauge. On/Off and automation `default` MUST keep the existing switch control. `userData` bool `default` MUST show the boolean state as text (ON/OFF), not a switch.

#### Scenario: Binary icon persists
- **WHEN** an editor saves a binaryInput binding with view style `icon` and icon `mdi:pump`
- **THEN** after reboot that binding still has those fields and view mode shows the pump icon

#### Scenario: On/Off icon persists
- **WHEN** an editor saves an On/Off binding with view style `icon` and icon `mdi:lightbulb`
- **THEN** after reboot that binding still has those fields

#### Scenario: Automation icon persists
- **WHEN** an editor saves an automation binding with view style `icon` and icon `mdi:timer`
- **THEN** after reboot that binding still has those fields

#### Scenario: UserData bool icon persists
- **WHEN** an editor saves a `userData` bool field binding with view style `icon` and icon `mdi:water`
- **THEN** after reboot that binding still has those fields and simple-user view mode shows the water icon

#### Scenario: Binary does not use gauge
- **WHEN** a binaryInput binding is edited
- **THEN** View style offers Default and Icon and does not offer Gauge

### Requirement: Icon view style is interactive for On/Off and automations
When view style is `icon` for an On/Off or automation binding, view mode MUST show the selected icon (not a switch) and MUST toggle the device ON/OFF or the automation `active` flag when the user activates the icon, using the same path as today’s switch. Binary sensor icons MAY stay display-only. ON/active MUST use yellow; OFF/inactive MUST use the muted theme color.

#### Scenario: User toggles On/Off via icon
- **WHEN** a simple user taps an On/Off binding shown as an icon while the device is OFF
- **THEN** the host sends ON for that device and channel the same way the switch would

#### Scenario: User toggles automation via icon
- **WHEN** a simple user taps an automation binding shown as an icon while the automation is active
- **THEN** that automation becomes inactive on the host

### Requirement: Editor icon preview is display-only for On/Off and automations
Editor preview of an On/Off or automation icon binding MUST NOT send device commands or change automation `active`.

#### Scenario: Editor icon is display-only
- **WHEN** an editor taps an On/Off or automation icon in the Consoles edit preview
- **THEN** no device command is sent and no automation `active` flag changes

### Requirement: Binding icon scale multiplies viewport fit
When a binding uses view style `icon`, the binding MUST store an optional numeric `iconScale` (default `1` when missing or invalid). In view mode the rendered icon size MUST equal the size produced by the widget viewport fit scale (grid cell, composite cell, or vertical row) **multiplied by** `iconScale`. `iconScale` MUST NOT replace or disable the viewport fit scale. Values less than or equal to zero MUST behave as `1`.

#### Scenario: Default icon scale is one
- **WHEN** an icon binding is saved without `iconScale`
- **THEN** after reload the icon uses only the viewport fit size (effective multiplier `1`)

#### Scenario: Icon scale multiplies grid fit
- **WHEN** an icon binding in a grid cell has `iconScale` `1.5` and the cell fit would make the icon 20px tall
- **THEN** the icon renders at 30px tall (1.5 × fit), not 1.5× some fixed base that ignores the cell

#### Scenario: Icon scale multiplies composite fit
- **WHEN** an icon binding in a composite cell has `iconScale` `2` and the cell fit would make the icon 16px tall
- **THEN** the icon renders at 32px tall

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

### Requirement: Content align fields persist
The host MUST persist `alignH` and `alignV` on device and automation bindings through save and reboot for every view style.

#### Scenario: Align round-trip for default view
- **WHEN** an editor saves a Default-view On/Off binding with `alignH` `left` and `alignV` `top`
- **THEN** after reboot those field values are unchanged

### Requirement: Inactive consoles are hidden from users
A console that is not active MUST NOT appear as a tab for simple users even if those users are assigned to it. Editors MUST still see inactive consoles in the Consoles table.

#### Scenario: Assigned but inactive
- **WHEN** user `alice` is assigned console `Garden` and `Garden` is not active
- **THEN** `alice` does not see a `Garden` tab

### Requirement: User page shows assigned active consoles as tabs
A simple user MUST see one top tab per active console assigned to that user. Each tab MUST show only that console’s widgets and devices. If the user has no visible consoles, the page MUST show that none are assigned.

#### Scenario: Two assigned consoles
- **WHEN** simple user `bob` is assigned active consoles `Kitchen` and `Hall`
- **THEN** the control page shows tabs `Kitchen` and `Hall` and selecting `Hall` shows only `Hall` content

#### Scenario: None assigned
- **WHEN** a simple user has no assigned active consoles
- **THEN** the page does not show control tabs with widgets

### Requirement: Widgets stack in a reorderable grid
Widgets MUST sit in a single vertical stack in edit mode. The editor MUST reorder widgets only up and down (drag onto another stack position or up/down controls). Left/right placement MUST NOT be offered. Spacer/empty placeholder widgets MUST NOT appear. A widget-styled add control MUST sit after the last widget and MUST move down when a widget is added.

#### Scenario: Add widget
- **WHEN** the editor activates the add-widget control
- **THEN** a new widget appears and the add control sits after it

#### Scenario: Drag widget
- **WHEN** the editor drops widget B onto widget A’s place in the vertical stack
- **THEN** the saved order places B where A was and no spacer widgets are inserted

#### Scenario: No left or right move
- **WHEN** the editor uses widget move controls
- **THEN** only up and down are available

### Requirement: Widget title and group On/Off
Each widget MUST have a title. The editor MUST set the title and whether the group On/Off control is shown. In view mode, if group On/Off is shown, it MUST be On when at least one bound On/Off device in that widget is On, and Off when all such devices are Off. Toggling group On/Off in view mode MUST switch every bound On/Off device in that widget. Coverings and sensors MUST NOT be included in that group control.

#### Scenario: Group on when any on
- **WHEN** a widget has two On/Off devices and one is On
- **THEN** the group switch shows On

#### Scenario: Group toggle all
- **WHEN** the user turns the group switch Off
- **THEN** every On/Off device in that widget is commanded Off

### Requirement: Device bindings match device class
An editor MUST add a device from registered devices, pick a channel when the device has more than one channel, and set a row title. When the selected binding parameter is a measurement-like reading, the editor MUST pick a unit from a list for that **parameter’s** measure kind (temperature, pressure, humidity, illuminance, flow, battery percent, and similar)—not from the device’s global classified type alone. On/Off rows MUST show a switch. Window covering rows MUST show up, down, stop, and current position. Sensor rows MUST show the live value in the chosen unit. An editor MUST also be able to add an automation binding from stored automations; that row MUST show an On/Off control reflecting the automation’s `active` flag (On = active).

#### Scenario: Multi-channel On/Off
- **WHEN** the editor adds a two-channel On/Off device and selects channel 2
- **THEN** the row controls only channel 2

#### Scenario: Automation row shows active as On/Off
- **WHEN** a widget binds automation `timer-light-on` that is active
- **THEN** the row switch shows On

#### Scenario: Temperature unit
- **WHEN** the editor adds a temperature sensor and selects the temperature / Sensor value parameter
- **THEN** the unit list includes Celsius, Fahrenheit, and Kelvin

#### Scenario: Battery param is not temperature units
- **WHEN** the editor binds a Measurement-class device and selects Battery level
- **THEN** the unit list is percent-oriented (or empty dash) and MUST NOT offer Celsius, Fahrenheit, or Kelvin as the only applicable set

#### Scenario: Humidity reportable on multi-value device
- **WHEN** the editor binds a device that also reports temperature and selects a humidity reportable parameter
- **THEN** the unit list is humidity percent, not temperature Celsius/Fahrenheit/Kelvin

### Requirement: Edit mode does not command devices
While a console is open in the editor dialog, the host MUST NOT send On/Off, covering, or other device commands from widget or row controls, and MUST NOT change automation `active` from those controls. View mode on the user page MUST send device commands using the same device actions as today, and MUST set automation `active` when the user toggles an automation row.

#### Scenario: Editor flips a switch
- **WHEN** the editor toggles an On/Off control inside the unsaved edit dialog
- **THEN** the device is not commanded and no automation `active` flag is changed

#### Scenario: User flips automation Off
- **WHEN** a simple user toggles an automation row Off on an assigned console
- **THEN** that automation becomes inactive on the host

#### Scenario: User flips a device switch
- **WHEN** a simple user toggles an On/Off device control on an assigned console
- **THEN** the host sends the On/Off command for that device and channel

### Requirement: Edit devices by click and drag
In edit mode the editor MUST reorder device rows by drag and drop and MUST open the device-binding dialog again by activating an existing row. The add-device control MUST be visible per widget in edit mode only.

#### Scenario: Reorder rows
- **WHEN** the editor drops device row 2 above row 1 and saves
- **THEN** the persisted widget lists that device first

#### Scenario: Edit binding
- **WHEN** the editor activates an existing row and changes its title
- **THEN** after save the row shows the new title

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

### Requirement: Simple users read layouts they are assigned
A simple-user session MUST receive layouts only for that user’s assigned active consoles. That session MUST NOT create, update, delete, import, or export console definitions. `editConsoles` or `isAdmin` MUST be required to mutate consoles.

#### Scenario: Simple user cannot save layout
- **WHEN** a simple-user session posts a console create or update
- **THEN** the host does not persist it

#### Scenario: Unassigned console hidden
- **WHEN** a simple-user session requests a console they are not assigned
- **THEN** the host does not return that console’s layout

### Requirement: Console data is heap-grown with Wi-Fi headroom
Console objects, widget lists, device bindings, and each user’s assigned-console list MUST be allocated at runtime (no compile-time arrays of consoles, widgets, or bindings). The host MUST refuse a create or grow that would leave too little free heap for AsyncWebServer and Wi‑Fi, and MUST free heap when a console, widget, binding, or assignment is removed.

#### Scenario: Grow then shrink
- **WHEN** an editor adds a widget and later deletes that widget and saves
- **THEN** that widget is gone after reload and its heap is released

#### Scenario: Low heap refuses grow
- **WHEN** free heap is at or below the reserved Wi‑Fi/HTTP floor and the editor adds a widget
- **THEN** the host does not add the widget and shows an error

### Requirement: Widget group On/Off ignores automation rows
The widget-level group On/Off control MUST continue to aggregate and command only bound On/Off **devices**. Automation bindings MUST NOT affect the group switch state and MUST NOT be toggled by the group switch.

#### Scenario: Group on from devices only
- **WHEN** a widget has one On/Off device that is Off and one automation that is active
- **THEN** the group switch shows Off

#### Scenario: Group toggle skips automations
- **WHEN** the user turns the group switch On
- **THEN** bound On/Off devices are commanded On and automation `active` flags are unchanged

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
In view mode, when the visible console content width is 13 cm or greater, visible widgets with `fullWidth` false or missing MUST fill a two-column grid left-to-right then top-to-bottom. Each such widget MUST take half of the row width and MUST NOT exceed 10 cm width. Widgets with `fullWidth` true MUST span 100% of the console content width (a full row). Saved console `columns` MUST NOT force more than two columns in this view mode for non-full-width widgets. Hidden widgets MUST NOT take a cell.

#### Scenario: Two-column fill order
- **WHEN** view mode shows four visible widgets with `fullWidth` false on a console at least 13 cm wide
- **THEN** widget order is top-left, top-right, bottom-left, bottom-right

#### Scenario: Widget max width
- **WHEN** view mode shows non-full-width widgets on a console wider than 20 cm
- **THEN** each widget column is at most 10 cm wide

#### Scenario: Saved many columns still two in view
- **WHEN** a console is persisted with `columns` greater than 2 and a simple user opens it in view mode at least 13 cm wide
- **THEN** visible non-full-width widgets still lay out in at most two columns under the width rules above

#### Scenario: Full width spans content
- **WHEN** view mode shows a widget with `fullWidth` true on a console at least 13 cm wide
- **THEN** that widget’s width is 100% of the console content width

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

### Requirement: Composite size calculation
For each composite row, `cm` cell widths MUST be fixed. Remaining widget width MUST split among `u` cells as `((Widget_width − Σ cm widths) / Σ u widths) × cell_u`. Row height in `cm` is fixed; row height in `u` MUST use that same unit size. Layout MUST NOT require equal columns.

#### Scenario: Mixed cm and unit widths
- **WHEN** widget content width is 10 cm, a row has cells 2 cm, 1 u, and 1 u
- **THEN** the first cell is 2 cm wide and each unit cell is 4 cm wide

#### Scenario: Unit row height follows width unit
- **WHEN** a row height is 1 u and that row’s computed unit size is 4 cm
- **THEN** the row height is 4 cm

### Requirement: Widget fullWidth attribute
Each real widget MUST store boolean `fullWidth` (default false). When false, view-mode width rules MUST match today’s adaptive layout. When true, width MUST be 100% of console content width. `fullWidth` MUST persist through save and reboot.

#### Scenario: Default not full width
- **WHEN** a widget is saved without `fullWidth`
- **THEN** after reload it behaves as `fullWidth` false

#### Scenario: Full width on narrow console
- **WHEN** view mode is under 13 cm wide and a widget has `fullWidth` true
- **THEN** that widget still uses 100% of the console content width

### Requirement: Composite cell drag reorder across rows
In composite edit mode the editor MUST let the operator drag a cell and drop it onto another position in the same row or in a different row of the same widget. After the drop, the editor MUST recalculate that widget’s composite row heights using the existing composite size rules and MUST remove a source row that became empty.

#### Scenario: Move cell to another row
- **WHEN** the editor drags a cell from row 1 onto row 2 and drops it
- **THEN** the draft places that cell in row 2 and row heights refresh for the affected rows

#### Scenario: Empty source row removed
- **WHEN** the editor moves the last cell out of a row
- **THEN** that empty row is removed from the draft

### Requirement: Numeric binding as-logical conversion
A device binding whose selected parameter is numeric (digital) MAY store boolean `asLogical` (default false when missing) and numeric `logicalThreshold` (default `0` when missing or invalid). When `asLogical` is true, view mode MUST convert the live numeric reading to a boolean with rule: value **less than** `logicalThreshold` is FALSE; value **greater than or equal to** `logicalThreshold` is TRUE. Missing or non-numeric live readings MUST treat as FALSE for icon/default logical display. When `asLogical` is false, the binding MUST keep existing numeric Default/Gauge behavior and MUST ignore `logicalThreshold` for display. Binary sensor types, On/Off status, automation bindings, and native `userData` bool fields are unchanged by this requirement (they already have bool semantics).

#### Scenario: Threshold converts to true
- **WHEN** a numeric binding has `asLogical` true, `logicalThreshold` `10`, and the live value is `10`
- **THEN** view mode treats the binding state as TRUE

#### Scenario: Below threshold is false
- **WHEN** a numeric binding has `asLogical` true, `logicalThreshold` `10`, and the live value is `9.9`
- **THEN** view mode treats the binding state as FALSE

#### Scenario: As-logical fields persist
- **WHEN** an editor saves a numeric binding with `asLogical` true and `logicalThreshold` `5.5`
- **THEN** after reboot those fields are unchanged

### Requirement: Numeric as-logical allows icon view style
When `asLogical` is true on a numeric binding, the binding MUST support view style `default` or `icon` (not `gauge`). Icon presentation MUST match existing bool/binary icon coloring (TRUE/active yellow; FALSE/inactive muted theme). Icon for as-logical numeric bindings MUST be display-only (MUST NOT send device commands). When `asLogical` is false, Icon MUST NOT be offered for that numeric param and Gauge MAY be offered as today. If a stored binding has `asLogical` false and `viewStyle` `icon`, view mode MUST treat view style as `default`.

#### Scenario: Icon offered when as logical
- **WHEN** an editor enables as logical on a temperature `sensorValue` binding
- **THEN** View style offers Default and Icon and does not offer Gauge

#### Scenario: Icon shows true yellow
- **WHEN** a numeric as-logical icon binding’s converted state is TRUE
- **THEN** view mode shows the selected icon in the active yellow color used for bool icons

#### Scenario: Gauge restored when as logical off
- **WHEN** the editor unchecks as logical on a numeric binding
- **THEN** View style offers Default and Gauge and does not offer Icon

### Requirement: Device binding may store a click action
A device binding on a console widget MAY optionally store one click action. When present, that action MUST use the same shape as an automation device/automation-target action (same fields the automation action editor persists). When omitted, the binding MUST keep today’s control behavior. Older console JSON without a click action MUST load with no click action.

#### Scenario: Click action persists
- **WHEN** the operator saves a binding with a click action that turns device B ON and the host reboots
- **THEN** that binding still has the same click action after reload

#### Scenario: Legacy binding without click action
- **WHEN** an older console JSON has a device binding with no click action field
- **THEN** view mode keeps the existing device control for that binding

### Requirement: Click action turns Place device into action button
When a device binding has a click action, Place and simple-user view mode MUST present that binding as an action button. Activating it MUST run the stored click action through the same execute path as automation actions. It MUST NOT perform the binding’s normal On/Off, covering, or other device-control gesture for that activation. Edit mode MUST still NOT send device commands or run click actions from widget controls.

#### Scenario: View mode runs click action
- **WHEN** a Place view-mode user activates a device binding that has a click action
- **THEN** the host runs that click action and does not toggle the bound device via the normal On/Off path

#### Scenario: Edit mode does not run click action
- **WHEN** the operator activates that same binding while the console editor dialog is open
- **THEN** the host does not run the click action and does not send device commands

#### Scenario: No click action keeps normal control
- **WHEN** a device binding has no click action and the user toggles On/Off in Place view mode
- **THEN** the host sends the normal On/Off command for that binding

### Requirement: Automation binding control mode
An automation binding MUST persist a control mode. Missing mode MUST behave as Enable/Disable. Allowed modes: Enable/Disable (default), Run, and when the bound automation has TIMER `use` true also Start timer, Stop timer, and Restart timer. The binding dialog MUST offer these as a closed dropdown (no free typing) for the selected automation.

#### Scenario: Default Enable/Disable
- **WHEN** an editor adds an automation binding without choosing a control mode
- **THEN** view mode uses Enable/Disable (active toggle) behavior

#### Scenario: Run mode button
- **WHEN** an editor saves control mode Run on an automation binding
- **THEN** view mode shows a button that requests Run for that automation once

### Requirement: Inactive automation appearance and action flash
When a bound automation is inactive, view mode MUST show the binding name in gray and the icon at 90% opacity. After the operator activates a non–Enable/Disable control (Run or a timer op) on an **enabled** automation, the icon MUST show the ON appearance for 0.6 seconds then return. When the automation is disabled, activating that control MUST NOT flash the icon.

#### Scenario: Inactive styling
- **WHEN** a bound automation is inactive and shown with an icon
- **THEN** the name is gray and the icon opacity is 90%

#### Scenario: Flash on Run when enabled
- **WHEN** the operator clicks Run on a widget automation binding whose automation is enabled
- **THEN** the icon shows ON for 0.6 seconds

#### Scenario: No flash when disabled
- **WHEN** the operator clicks Run on a widget automation binding whose automation is disabled
- **THEN** the icon does not flash

### Requirement: Grid and Composite editor row move arrows
In the Consoles widget editor, when View style is Composite, each composite row MUST offer small up and down arrow buttons to reorder rows. First-row up and last-row down MUST be disabled or no-op. When View style is Grid, binding order MUST be changed by drag-and-drop only (no up/down arrow buttons on grid cells).

#### Scenario: Move composite row up
- **WHEN** the editor clicks the up arrow on the second composite row
- **THEN** that row swaps with the row above it in the draft

#### Scenario: Grid has no cell move arrows
- **WHEN** View style is Grid and the editor views a binding cell chrome
- **THEN** up/down arrow buttons for that cell are not shown

### Requirement: Composite text field parse is not confused by content text
When loading a composite cell whose JSON includes `"content":"text"` and a separate `"text"` string value, the host MUST treat only the `"text"` key’s value as the cell display text. A string value equal to `text` on another key (including `content`) MUST NOT be read as the cell’s display text.

#### Scenario: Content value text does not steal text key
- **WHEN** stored cell JSON has `"content":"text","alignH":"center","text":"Hello"` (any key order with those fields)
- **THEN** after load the cell content is text and the display text is `Hello`

### Requirement: Binding unit follows selected parameter
For a device binding, the unit field visibility and option list MUST be derived from the currently selected parameter (Sensor value using that device’s measurement class, reportable cluster-mapped measure kind, battery, or other property with a known unit set). Changing the parameter MUST refresh the unit options. A unit appropriate only for another property on the same device MUST NOT remain the sole applicable choice for the newly selected parameter.

#### Scenario: Switch param from temperature to humidity
- **WHEN** a binding’s unit list shows temperature units for Sensor value / temperature and the editor changes param to a humidity reading on the same device
- **THEN** the unit options refresh to humidity percent before Save

#### Scenario: Non-measurement param hides engineering units
- **WHEN** the editor selects a non-measurement binding parameter such as On/Off Status
- **THEN** the measurement unit field is hidden
