# Spec Delta

## ADDED Requirements

### Requirement: Email action optional camera snapshot
An email action MAY attach zero or more camera snapshots (same Add/row/remove pattern as email device parameters, at most eight). Each attach MUST persist camera id and channel (1 or 2). At run time the host MUST fetch each channel’s snapshot HTTP URL into a PSRAM-only buffer and pass all successful JPEGs to outbound email as attachments. When the attach list is empty, behavior MUST match today’s text-only email action. Fetch or allocate failure for one camera MUST log a warning and MUST NOT block other attaches or the text body when SMTP is configured.

#### Scenario: Persist multiple attaches
- **WHEN** the operator saves an e-mail action with two camera snapshot rows
- **THEN** reload and export still contain both attach rows

#### Scenario: Run attaches snapshot
- **WHEN** an email action with attach enabled runs, the account is configured, and the snapshot fetch into PSRAM succeeds
- **THEN** the outbound send includes a JPEG attachment from that channel

#### Scenario: Fetch failure still sends text
- **WHEN** snapshot fetch fails and the account is configured
- **THEN** the log shows a warning and the text email still starts without an image attachment

## MODIFIED Requirements

### Requirement: Email action type
An automation action MUST support target type `e-mail` in addition to device and automation targets. An email action MUST persist receivers (semicolon-separated address list), theme (subject), message (body text), an ordered parameter list of at most eight entries, and optional camera snapshot attach fields (enabled flag, camera id, channel). Each parameter MUST persist device identity, attribute binding (known parameter or equivalent status binding used to read live value), and title. Email-action fields MUST round-trip through save, list, import, export, and reboot.

#### Scenario: Persist email action
- **WHEN** the operator saves an automation whose action is `e-mail` with receivers, theme, message, and two parameters
- **THEN** reload and export still contain that email action with the same receivers, theme, message, and both parameters

#### Scenario: Max eight parameters
- **WHEN** the operator attempts to save an email action with more than eight parameters
- **THEN** the host rejects the save or truncates to eight according to validation (save MUST NOT store more than eight)

#### Scenario: Persist optional camera attach
- **WHEN** the operator saves an e-mail action with camera attach on channel 1
- **THEN** reload still has attach enabled for that camera and channel 1
