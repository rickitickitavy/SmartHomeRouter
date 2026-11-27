## MODIFIED Requirements

### Requirement: Async outbound send
When a caller requests send with a configured account, the host MUST enqueue or otherwise start the SMTP session off the automation action path so the host service loop is not blocked for the full SMTP conversation. The send MUST use the stored account settings. Delivery success or failure MUST be logged on the firmware log path operators read from System → Log.

#### Scenario: Send does not block action return
- **WHEN** an automation email action runs with a configured account
- **THEN** the action path returns without waiting for the full SMTP session to finish

#### Scenario: Failure is logged
- **WHEN** an async send fails (connect, auth, or server reject)
- **THEN** the firmware log includes a warning or error describing the failure

## ADDED Requirements

### Requirement: SMTP on ESP-IDF hosts
On ESP32-S3 and ESP32-P4 ESP-IDF host images, outbound email MUST still satisfy the configured-account gate, async send, and optional PSRAM JPEG attachment requirements. The host MUST NOT require the Arduino framework to send SMTP.

#### Scenario: IDF host sends without Arduino framework
- **WHEN** a configured account send is enqueued on an ESP-IDF host build
- **THEN** the SMTP path runs without linking the Arduino framework for that send
