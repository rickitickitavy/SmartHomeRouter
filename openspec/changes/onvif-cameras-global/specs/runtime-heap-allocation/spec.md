# Spec Delta

## ADDED Requirements

### Requirement: Camera settings and images stay off internal RAM
Camera registry records and their owned strings MUST be allocated from PSRAM. Any host-captured or host-held camera image (including email snapshot fetch) MUST be allocated only from PSRAM. The host MUST NOT keep those image bytes on internal RAM. If PSRAM cannot satisfy the allocation, the host MUST fail that path without falling back to internal RAM for the image or registry blob.

#### Scenario: Registry in PSRAM
- **WHEN** the camera registry loads on a host with PSRAM
- **THEN** camera records are not kept as an internal-heap-resident table

#### Scenario: Image never on internal RAM
- **WHEN** the host fetches a camera JPEG for email attach
- **THEN** the JPEG buffer is PSRAM-backed or the fetch is abandoned without an internal-RAM image buffer
