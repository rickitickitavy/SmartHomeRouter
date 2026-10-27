# outbound-email Specification

## Purpose

Host outbound SMTP account configuration and asynchronous email send so automations and other host features can notify operators without blocking the main loop.

## Requirements

### Requirement: Email account settings persist on the host
The host MUST persist an email account settings group with: SMTP host, SMTP port, encryption mode (none, STARTTLS, SSL/TLS), authentication enabled flag, username, password, From address, From display name, Reply-To address, EHLO/HELO hostname, connection timeout, and send timeout. After reboot the same values MUST load. Empty optional fields MUST remain allowed where not required for “configured”.

#### Scenario: Survives host reboot
- **WHEN** the operator saves a full email account and the host reboots
- **THEN** host, port, encryption, auth flag, username, password, From, display name, Reply-To, EHLO hostname, and timeouts are still present

#### Scenario: Optional fields may be empty
- **WHEN** the operator saves with Reply-To and EHLO hostname empty but required configured fields filled
- **THEN** the host accepts the save and stores those optionals as empty

### Requirement: Email account configured gate
The email account MUST be treated as configured only when SMTP host is non-empty, SMTP port is valid (1–65535), From address is non-empty, and when authentication is enabled both username and password are non-empty. Encryption and timeouts MUST use stored values (or defaults) when evaluating configured. An unconfigured account MUST NOT start an SMTP send.

#### Scenario: Missing host is not configured
- **WHEN** From is set but SMTP host is empty
- **THEN** the account is not configured and no SMTP send starts

#### Scenario: Auth required needs credentials
- **WHEN** authentication is enabled and username is empty
- **THEN** the account is not configured

#### Scenario: Auth off allows empty credentials
- **WHEN** authentication is disabled, host, port, and From are set
- **THEN** the account is configured even if username and password are empty

### Requirement: Async outbound send
When a caller requests send with a configured account, the host MUST enqueue or otherwise start the SMTP session off the automation action path so the main loop is not blocked for the full SMTP conversation. The send MUST use the stored account settings. Delivery success or failure MUST be logged on the firmware log path operators read from System → Log.

#### Scenario: Send does not block action return
- **WHEN** an automation email action runs with a configured account
- **THEN** the action path returns without waiting for the full SMTP session to finish

#### Scenario: Failure is logged
- **WHEN** an async send fails (connect, auth, or server reject)
- **THEN** the firmware log includes a warning or error describing the failure
