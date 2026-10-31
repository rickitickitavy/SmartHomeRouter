# Spec Delta

## Purpose

Host-side ONVIF camera registry for console display and email snapshots: identities, URLs, viewer ACL, status, and PSRAM-backed settings without holding console media on the ESP.

## ADDED Requirements

### Requirement: Camera registry persists
The host MUST persist a camera registry. Each camera MUST have a unique id, name, active flag, type (enum; ONVIF only in this change), primary display URL, ordered viewer user names, and type-specific settings. After reboot the same cameras MUST load. Registry records in RAM MUST live in PSRAM, not internal heap.

#### Scenario: Survives reboot
- **WHEN** the operator saves two ONVIF cameras and the host reboots
- **THEN** both cameras reload with the same name, active, type, URLs, users, and ONVIF settings

#### Scenario: Registry not on internal heap
- **WHEN** cameras load successfully on a host with PSRAM
- **THEN** camera records and their owned strings reside in PSRAM

### Requirement: No pre-add of cameras
Add MUST open an in-memory editor only. The camera MUST appear in the registry and list APIs only after the first successful Save. Cancel MUST discard the draft with no create and no orphan row.

#### Scenario: Cancel add leaves no row
- **WHEN** the operator clicks Add, enters a name, then Cancel without Save
- **THEN** the cameras table and list API do not contain that camera

### Requirement: ONVIF type settings and two channels
For type ONVIF, settings MUST include credentials/endpoint fields needed to reach the camera and exactly two channels. Each channel MUST store a snapshot HTTP URL and a live HTTP URL (browser-playable). Type MUST be chosen from a fixed dropdown; free-text type MUST NOT be allowed.

#### Scenario: Two channel URLs
- **WHEN** the operator saves an ONVIF camera with channel 1 and channel 2 snapshot and live URLs
- **THEN** reload returns both channels with those four URLs

#### Scenario: Type is dropdown only
- **WHEN** the operator opens the camera dialog
- **THEN** Type offers ONVIF and does not accept a typed custom type string

### Requirement: Camera status values
Status MUST be Offline when the camera is not active. When active, status MUST be Online when a lightweight reachability check succeeds, or a short error string when it fails. Reachability MUST NOT store a captured image in internal RAM.

#### Scenario: Inactive is offline
- **WHEN** a camera is not active
- **THEN** status shown to the operator is Offline

#### Scenario: Active unreachable shows error
- **WHEN** a camera is active and the reachability check fails with a short reason
- **THEN** status shows that short error (not Online)

### Requirement: No per-camera users ACL
Cameras MUST NOT store a per-camera users list. Any signed-in user who can open a console that contains a camera cell MAY receive that camera’s media metadata.

#### Scenario: Signed-in console user
- **WHEN** a signed-in simple user opens a console cell bound to camera C
- **THEN** the session receives the configured HTTP URLs needed to render the cell

### Requirement: Browser-direct console media
Console image and live display MUST use the camera’s HTTP URLs in the browser (direct fetch to the camera). The host MUST NOT proxy or buffer console image/video bytes for display.

#### Scenario: Console does not proxy JPEG
- **WHEN** Place or the simple-user page shows a camera cell in JPEG refresh mode
- **THEN** the browser loads the snapshot URL from the camera host and the gateway does not stream that JPEG body as a media proxy

### Requirement: Import and export cameras
Export MUST download the camera registry as JSON. Import MUST replace or merge per the Cameras UI confirm rules and MUST round-trip ONVIF fields. Import MUST NOT create draft rows without Save semantics for interactive Add. Legacy `users` arrays in import files MUST be ignored.

#### Scenario: Export then import
- **WHEN** the operator exports cameras and imports that file on another host with empty registry
- **THEN** the imported cameras match names, active, type, URLs, and channel settings

### Requirement: Snapshot fetch for email uses PSRAM only
When the host fetches a camera snapshot for an email attachment, image bytes MUST be allocated only in PSRAM. The host MUST NOT allocate or hold that image in internal RAM. After send attempt, the buffer MUST be freed. If PSRAM cannot hold the image, the host MUST skip the attachment, log a warning, and still send the text body when SMTP is otherwise configured.

#### Scenario: Attachment buffer in PSRAM
- **WHEN** an email action attaches channel 1 of an ONVIF camera and PSRAM can hold the JPEG
- **THEN** the JPEG bytes reside in PSRAM until freed after the send path finishes with them

#### Scenario: No PSRAM skips attachment
- **WHEN** PSRAM cannot allocate the snapshot buffer
- **THEN** the log shows a warning, no internal-RAM image buffer is kept, and the text email still proceeds if the account is configured
