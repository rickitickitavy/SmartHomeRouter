# Spec Delta

## ADDED Requirements

### Requirement: Email action type
An automation action MUST support target type `e-mail` in addition to device and automation targets. An email action MUST persist receivers (semicolon-separated address list), theme (subject), message (body text), and an ordered parameter list of at most eight entries. Each parameter MUST persist device identity, attribute binding (known parameter or equivalent status binding used to read live value), and title. Email-action fields MUST round-trip through save, list, import, export, and reboot.

#### Scenario: Persist email action
- **WHEN** the operator saves an automation whose action is `e-mail` with receivers, theme, message, and two parameters
- **THEN** reload and export still contain that email action with the same receivers, theme, message, and both parameters

#### Scenario: Max eight parameters
- **WHEN** the operator attempts to save an email action with more than eight parameters
- **THEN** the host rejects the save or truncates to eight according to validation (save MUST NOT store more than eight)

### Requirement: Email action body includes live parameter lines
When an email action runs, the host MUST build the outbound body as the stored message text followed by one line per configured parameter in order. Each line MUST be `title: <live value>` where `<live value>` is the current interpreted status for that parameter’s device and attribute. The device name MUST NOT be printed on the line. Missing device or unread attribute MUST still emit the line with an empty value after the colon (or a single space), without aborting other parameters.

#### Scenario: Two parameter lines
- **WHEN** an email action has message `Alert` and parameters title `Leak` and `Battery` with live values `LEAK` and `80`
- **THEN** the outbound body contains `Alert` and then lines `Leak: LEAK` and `Battery: 80`

#### Scenario: Missing live value still emits title line
- **WHEN** a parameter’s device is missing at send time and its title is `Temp`
- **THEN** the body still includes a `Temp:` line and later parameters are still processed

### Requirement: Email action uses PSRAM for its config
While an automation is loaded in memory, the entire `e-mail` action configuration (action node, receivers, theme, message, parameter structures, and their strings) MUST be allocated from PSRAM (SPIRAM), not from the internal heap used for ordinary automation device/automation actions. If PSRAM is unavailable at allocation time, the host MUST fail that email action parse/load with an out-of-memory style error rather than silently placing that email-action config on internal heap.

#### Scenario: Email action config not on internal heap
- **WHEN** an automation with an email action loads successfully on a host with PSRAM
- **THEN** that action’s node, receivers, theme, message, and parameter title/device/attribute strings reside in PSRAM

#### Scenario: No PSRAM rejects email action load
- **WHEN** PSRAM cannot satisfy an email-action allocation during parse
- **THEN** that automation load/save for the email action fails without keeping those email fields on internal RAM

### Requirement: Unconfigured email skips action with warning
When an email action runs and the outbound email account is not configured, the host MUST NOT start SMTP send for that action, MUST log a warning on the firmware log path, and MUST continue with any following actions in the same automation.

#### Scenario: Skip when not configured
- **WHEN** an automation’s first action is email and the account is not configured, and a second device action follows
- **THEN** the log shows a warning, no SMTP send starts for the email action, and the second action still runs

#### Scenario: Configured email proceeds
- **WHEN** an email action runs and the account is configured
- **THEN** the host starts an async outbound send using receivers, theme, and the built body
