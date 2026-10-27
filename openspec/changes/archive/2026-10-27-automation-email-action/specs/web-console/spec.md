# Spec Delta

## ADDED Requirements

### Requirement: Network E-mail tab
The Network section MUST include an **E-mail** subtab alongside WiFi, MQTT, and ZigBee. Selecting **E-mail** MUST show the E-Mail settings card and hide the other Network tab panels without a full page reload. Only `isAdmin` MUST see Network settings including E-mail (same gate as other Network tabs).

#### Scenario: Open E-mail tab
- **WHEN** the operator opens Network and selects E-mail
- **THEN** the E-Mail card is visible and WiFi/MQTT/ZigBee panels are hidden

### Requirement: E-Mail card edits the email account group
The E-Mail card MUST show and allow saving the outbound email account fields (SMTP host, port, encryption, auth flag, username, password with show/hide, From, display name, Reply-To, EHLO hostname, connection and send timeouts). Load MUST use `GET /api/email`; Save MUST use `POST /api/email`.

#### Scenario: Load email settings
- **WHEN** the operator opens the E-mail tab and the host has a stored SMTP host
- **THEN** the card fields show the stored email account values

#### Scenario: Save email settings
- **WHEN** the operator enters a valid host, port, From, and encryption and saves
- **THEN** the email group is persisted and a later GET returns those values

### Requirement: E-Mail card Reset and footer
Reset on the E-Mail card MUST reload stored values and MUST NOT write settings. Save and Reset MUST remain reachable (scroll or pinned footer like other Network cards).

#### Scenario: Reset reloads without write
- **WHEN** the operator edits a field then clicks Reset
- **THEN** the form shows the last stored values and no new settings write occurs from Reset alone

### Requirement: Email action editor in automations
When an action target is **e-mail**, the Actions row MUST show receivers (text; semicolon-separated), theme (subject), message (multiline body), and a parameter list with Add/Remove for at most eight rows. Each parameter row MUST let the operator pick device, attribute (same known-parameter-or-CUSTOM pattern as Trigger for reading status), and title. The row MUST NOT collect a static value field for the parameter. Switching target away from e-mail MUST hide email-specific fields.

#### Scenario: Email action fields visible
- **WHEN** the operator sets an action target to e-mail
- **THEN** receivers, theme, message, and the parameter list controls are visible

#### Scenario: Parameter uses device attribute title
- **WHEN** the operator adds an email parameter row
- **THEN** the row offers device, attribute, and title inputs and does not offer a static value input

#### Scenario: Eighth parameter is the last Add
- **WHEN** the email action already has eight parameters
- **THEN** Add is disabled or does not create a ninth row

## MODIFIED Requirements

### Requirement: Automation edit dialog import and export
Add or row edit MUST open an edit dialog with **General** (Active, then Name, then Group, Debug; no title), **Trigger** (countdown TIMER controls first, then listening devices with typed parameter pickers), Condition (enable checkbox plus editor when enabled, with typed comparison pickers), actions (device actions with typed pickers, automation targets, or **e-mail**), Export, and Import. Dialog Import MUST be enabled only when the form is empty (no listeners, no actions, condition disabled or empty, TIMER `use` false / defaults unset). Dialog Export MUST download one JSON file named from the automation’s `name`.

#### Scenario: Group after name
- **WHEN** the operator opens the automation edit dialog
- **THEN** the Group field appears immediately below Name and accepts typed text or a choice from existing groups

#### Scenario: Empty-only dialog import
- **WHEN** the edit dialog already has a listener, action, or TIMER `use` true
- **THEN** Import is disabled

#### Scenario: No Title field
- **WHEN** the operator opens the automation edit dialog
- **THEN** there is no Title input

#### Scenario: Email among action targets
- **WHEN** the operator opens the Actions editor target control
- **THEN** the options include device, automation, and e-mail

### Requirement: Automation actions target devices or automations
The edit dialog Actions editor MUST let each action target a device (payload/channel as Manual command), an automation, or **e-mail**. Automation-target ops MUST include ENABLE and DISABLE; when the selected target automation uses a timer, ops MUST also include STOP TIMER, START TIMER, and RESTART TIMER. START TIMER MUST allow an optional countdown time when the target has no default time. An **e-mail** target MUST use the email action editor fields (receivers, theme, message, parameters).

#### Scenario: Automation action ops
- **WHEN** the operator adds an action targeting an automation that uses a timer
- **THEN** the op list includes ENABLE, DISABLE, STOP TIMER, START TIMER, and RESTART TIMER

#### Scenario: Device action unchanged shape
- **WHEN** the operator adds a device-target action
- **THEN** the row uses device, channel, and payload controls like Manual command

#### Scenario: Email target shape
- **WHEN** the operator adds an action targeting e-mail
- **THEN** the row shows receivers, theme, message, and the email parameter list
