# Spec Delta

## MODIFIED Requirements

### Requirement: Camera cell on Place and simple-user
Place view-mode and the simple-user page MUST render camera cells the same way: JPEG refresh uses the channel snapshot URL with periodic reload; live uses the channel live HTTP URL in the browser. While a JPEG refresh is in progress, the cell MUST keep showing the last successfully loaded frame and MUST NOT blank or flash empty between frames. Sync rule: behavioral changes MUST land on both pages.

#### Scenario: JPEG refresh on both surfaces
- **WHEN** a console with a JPEG-refresh camera cell is opened on Place and on the simple-user page by an allowed user
- **THEN** both surfaces show the image from the camera snapshot URL with refresh behavior

#### Scenario: JPEG refresh keeps last frame
- **WHEN** a JPEG-refresh camera cell already shows a snapshot and the next periodic reload starts
- **THEN** the previous snapshot remains visible until the new snapshot finishes loading successfully

#### Scenario: Failed JPEG refresh keeps last frame
- **WHEN** a JPEG-refresh camera cell already shows a snapshot and the next reload fails
- **THEN** the previous snapshot remains visible

#### Scenario: Live console refresh does not remount all cameras
- **WHEN** Place or the simple-user page refreshes console controls because a device status arrived
- **THEN** existing camera cells keep their current frames instead of blanking and reloading together

#### Scenario: Live mode uses live URL
- **WHEN** the cell mode is live video and channel 1 has an HTTP live URL
- **THEN** the browser loads that live URL (not the snapshot URL) for display

#### Scenario: Live mode rejects non-HTTP URL
- **WHEN** the cell mode is live video and the channel live URL is RTSP or otherwise not http(s)
- **THEN** the console shows that Live needs an HTTP URL and MUST NOT load the snapshot URL instead
