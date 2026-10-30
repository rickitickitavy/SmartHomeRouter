# Spec Delta

## MODIFIED Requirements

### Requirement: Log tab shows recent firmware logs

The Log tab MUST display recent firmware log lines that are also emitted on USB when console logging is enabled. The page MUST be able to refresh that list without leaving the Log tab. Older lines MAY drop when the in-memory buffer is full. The Refresh control MUST stay visible without a second page scroller hiding it, MUST be left-aligned, and MUST use a compact content width (not a full-width bar). The Log card MUST NOT repeat a second “Log” title under the Log tab. The on-screen list MAY show only a bounded tail (on the order of 64 KiB) for UI performance. A **Download full log** control MUST download the entire current ring used length (up to ring capacity, typically the PSRAM ring), not only that on-screen tail. The download MUST use a dedicated full-ring HTTP path that MUST NOT be handled by the bounded view-tail endpoint (including when that view path is a prefix of the download path).

#### Scenario: New log line appears

- **WHEN** the firmware emits a log line and the operator refreshes the Log tab (automatically or manually)
- **THEN** that line appears in the log view (unless it has already aged out of the buffer)

#### Scenario: USB logging still works

- **WHEN** a log line is emitted
- **THEN** it still appears on the USB serial console when USB debug logging is enabled

#### Scenario: Refresh stays visible

- **WHEN** the operator opens System → Log
- **THEN** Refresh is visible, left-aligned, and not stretched across the card

#### Scenario: No duplicate Log title
- **WHEN** the operator opens System → Log
- **THEN** the tab is labeled Log and the card does not show another Log heading

#### Scenario: Download exceeds view cap when ring is larger
- **WHEN** the host log ring holds more than 64 KiB and the operator activates Download full log
- **THEN** the downloaded file size matches the ring’s used length (greater than 64 KiB when the ring holds more)

#### Scenario: Download equals ring when small
- **WHEN** the ring holds less than 64 KiB and the operator activates Download full log
- **THEN** the downloaded file contains the full current ring text

#### Scenario: Download path is not the view-tail response
- **WHEN** the operator activates Download full log while the ring holds more than the on-screen view cap
- **THEN** the response is the full-ring download (attachment of used length), not the bounded view-tail body capped near 64 KiB
