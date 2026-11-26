## ADDED Requirements

### Requirement: Full backup streams ZIP with bounded RAM buffer
When Maintenance Full backup downloads the ZIP, the host SHALL generate and send the archive using a fixed work buffer of **4 KiB to 8 KiB** and SHALL NOT keep the complete ZIP (all members plus central directory as one assembled blob) resident in RAM for the download. The downloaded ZIP MUST remain a valid store-method archive with the same members and operator options as today’s Full backup (settings, optional Wi‑Fi / secrets / history, scene bake JPEGs, manifest). Ordinary Export settings behavior SHALL remain unchanged. Full restore upload behavior is out of scope for this requirement.

#### Scenario: No whole-archive RAM buffer during download
- **WHEN** an admin runs Full backup with typical settings and optional history/bakes
- **THEN** the host does not allocate or retain a single buffer holding the finished ZIP for the HTTP response, and archive generation uses only a 4–8 KiB work/send buffer plus normal per-member source buffers

#### Scenario: Free RAM stays usable
- **WHEN** Full backup completes successfully
- **THEN** Status Free RAM is not left depressed by a retained whole-archive buffer (any temporary member-source buffers used during the stream are released after the response finishes or aborts)

#### Scenario: ZIP still restores
- **WHEN** the streamed Full backup ZIP is later used with Full restore
- **THEN** the host accepts the archive the same way as a previously built store-method Full backup ZIP

### Requirement: Camera Resolve/Test Free RAM stack cap
When Camera Resolve or Camera Test Snapshots runs on the dedicated `camOnvif` worker, the host SHALL size that worker’s FreeRTOS stack (Free RAM) to **16 KiB** when every relevant request URL is HTTP, and to **32 KiB** when any relevant URL is HTTPS. Relevant URLs are the request body’s `url`, `snapshotUrl`, `snapshotUrl1`, and `snapshotUrl2` fields. The host SHALL NOT use a 64 KiB stack for this worker.

#### Scenario: HTTP Resolve uses 16 KiB stack
- **WHEN** an admin runs Camera Resolve with only `http://` device URLs
- **THEN** the `camOnvif` worker is created with a 16 KiB stack

#### Scenario: HTTPS Test uses 32 KiB stack
- **WHEN** an admin runs Camera Test Snapshots with an `https://` snapshot URL
- **THEN** the `camOnvif` worker is created with a 32 KiB stack
