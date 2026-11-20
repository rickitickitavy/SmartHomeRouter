## MODIFIED Requirements

### Requirement: History query API for charts
The host SHALL expose an authenticated history query API that returns samples for one series over a time range, suitable for chart rendering, with a bounded maximum point count of **256** (downsample or truncate as needed; default and clamp MUST NOT exceed 256). Numeric series SHALL be chartable as a step-held line; binary/state series SHALL be chartable as colored state bands.

#### Scenario: Range query returns points
- **WHEN** a signed-in user requests history for an enabled series over the last day and samples exist
- **THEN** the API returns a time-ordered list of samples within the requested range up to the point bound

#### Scenario: Client may request up to 256 points
- **WHEN** the client passes `maxPoints=256`
- **THEN** the host returns at most 256 samples for that query

#### Scenario: Over-limit maxPoints is clamped
- **WHEN** the client passes `maxPoints` greater than 256
- **THEN** the host still returns at most 256 samples
