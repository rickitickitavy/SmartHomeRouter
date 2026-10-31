# Spec Delta

## ADDED Requirements

### Requirement: Optional JPEG attachment from PSRAM
Async outbound send MUST accept an optional JPEG attachment whose bytes are provided in a PSRAM buffer. The send path MUST NOT copy that image into internal RAM. After the SMTP attempt finishes with the attachment, the buffer MUST be released. When no attachment is provided, text-only send MUST behave as today.

#### Scenario: Send with PSRAM JPEG
- **WHEN** a caller enqueues send with a configured account and a PSRAM-backed JPEG buffer
- **THEN** the async SMTP path includes that JPEG as an attachment without placing a second full image copy on internal heap

#### Scenario: Text-only unchanged
- **WHEN** a caller enqueues send with theme and body and no attachment
- **THEN** the async send proceeds as a text email as before this change
