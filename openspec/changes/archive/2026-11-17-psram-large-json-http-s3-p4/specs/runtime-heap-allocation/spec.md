# Spec Delta

## ADDED Requirements

### Requirement: Large JSON text stays in PSRAM on S3 and P4
On the ESP32-S3 and ESP32-P4 hosts, JSON text larger than 4096 bytes MUST be allocated from PSRAM. That includes generated documents and the working text used to parse those documents. The host MUST NOT place that large text on internal heap, including when a PSRAM allocation fails. JSON at or below 4096 bytes MAY use internal heap. The slave MUST NOT apply this rule.

#### Scenario: Large generated document uses PSRAM
- **WHEN** an S3 or P4 host builds a JSON document larger than 4096 bytes
- **THEN** that document’s text resides in PSRAM and is not copied as a whole onto internal heap

#### Scenario: Large parse text uses PSRAM
- **WHEN** an S3 or P4 host parses a JSON document larger than 4096 bytes
- **THEN** the document text being parsed resides in PSRAM for the parse

#### Scenario: PSRAM failure does not use internal heap
- **WHEN** an S3 or P4 host cannot allocate PSRAM for JSON text larger than 4096 bytes
- **THEN** that text is not allocated on internal heap and the operation fails

#### Scenario: Small JSON may use internal heap
- **WHEN** an S3 or P4 host builds or parses JSON of 4096 bytes or less
- **THEN** that text MAY reside on internal heap

### Requirement: Large HTTP bodies stay in PSRAM on S3 and P4
On the ESP32-S3 and ESP32-P4 hosts, an HTTP body whose allowed maximum is above 4096 bytes MUST be accumulated in PSRAM. The host MUST NOT accumulate that body on internal heap. If PSRAM cannot hold the body, the host MUST fail that request without an internal-heap body. The slave MUST NOT apply this rule.

#### Scenario: Body collector uses PSRAM
- **WHEN** an S3 or P4 host receives an HTTP body on a route whose maximum is above 4096 bytes
- **THEN** the collected body bytes reside in PSRAM

#### Scenario: Body PSRAM failure
- **WHEN** an S3 or P4 host cannot allocate PSRAM for such an HTTP body
- **THEN** the host does not store that body on internal heap and does not apply it
