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
