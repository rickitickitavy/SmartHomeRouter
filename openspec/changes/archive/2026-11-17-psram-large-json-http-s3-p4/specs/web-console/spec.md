# Spec Delta

## ADDED Requirements

### Requirement: Large HTTP JSON bodies are parsed from PSRAM
On the ESP32-S3 and ESP32-P4 hosts, each HTTP JSON POST body whose route maximum is above 4096 bytes MUST be collected in PSRAM and parsed from that text. The host MUST NOT copy the whole body onto internal heap to parse it.

#### Scenario: Large save body is parsed from PSRAM
- **WHEN** an admin posts a JSON body larger than 4096 bytes and within the route maximum
- **THEN** the host parses that body from PSRAM and applies it with the same result as before

### Requirement: HTTP JSON body PSRAM failure
On the ESP32-S3 and ESP32-P4 hosts, if PSRAM cannot hold an HTTP JSON POST body, the host MUST respond with HTTP 507, MUST NOT apply the body, and MUST NOT store that body on internal heap.

#### Scenario: PSRAM exhaustion on a body
- **WHEN** an S3 or P4 host cannot allocate PSRAM for a JSON POST body
- **THEN** the host responds with HTTP 507 and does not apply that body

### Requirement: Large JSON HTTP responses are sent from PSRAM
On the ESP32-S3 and ESP32-P4 hosts, a JSON HTTP response whose document is larger than 4096 bytes MUST be generated in PSRAM and sent from that text. The host MUST NOT copy the full document onto internal heap before sending. The JSON fields MUST remain compatible with the existing console.

#### Scenario: Large list still renders
- **WHEN** a signed-in operator loads a JSON list or export larger than 4096 bytes
- **THEN** the response body is the same logical JSON as before and was not held as a full copy on internal heap

#### Scenario: Scene view is sent from PSRAM
- **WHEN** a signed-in user loads a scene view whose JSON is larger than 4096 bytes
- **THEN** the response fields stay id, name, bakePath, and pins, and that document is not held as a full copy on internal heap

### Requirement: Large JSON response PSRAM failure
On the ESP32-S3 and ESP32-P4 hosts, if PSRAM cannot hold a JSON response larger than 4096 bytes, the host MUST respond with HTTP 507 and MUST NOT build that document on internal heap.

#### Scenario: PSRAM exhaustion on a response
- **WHEN** an S3 or P4 host cannot allocate PSRAM for a JSON response larger than 4096 bytes
- **THEN** the host responds with HTTP 507 and does not allocate that document on internal heap
