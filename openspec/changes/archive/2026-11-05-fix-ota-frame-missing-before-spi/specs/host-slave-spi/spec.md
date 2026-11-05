## ADDED Requirements

### Requirement: Firmware-OTA retry must not send or fail on an empty last frame

While the host is streaming a slave firmware-OTA sequence (begin / chunk / end), a missing or late slave reply MAY trigger a resend of the **same** last OTA frame that was previously packed. If that last-frame buffer is empty (length zero), the host SHALL NOT enqueue a zero-length firmware-OTA command and SHALL NOT treat that empty buffer alone as grounds to abort the entire firmware-update sequence when begin has not yet been successfully staged. After begin has been staged, an empty buffer on retry SHALL either wait for the next pack from the image stream or fail with a distinct, accurate error after the allowed frame-loss budget—never invent a synthetic SPI payload.

#### Scenario: Timeout with empty buffer before begin staged

- **WHEN** the host is in the slave firmware-OTA streaming phase, begin has not been successfully staged into the last-frame buffer, and a timeout or NACK retry path runs
- **THEN** the host does not enqueue a firmware-OTA frame with length zero and does not abort solely with “Firmware OTA frame missing”

#### Scenario: Resend uses the packed last frame

- **WHEN** a slave firmware-OTA frame was packed into the last-frame buffer and a missing or late reply triggers a resend within the allowed fail budget
- **THEN** the host re-enqueues that same packed payload and sequence as today

#### Scenario: Concurrent pump and SPI event agree on buffer lifetime

- **WHEN** the host Arduino loop pumps firmware-OTA progress while the host SPI task delivers OTA command results or timeouts for the same transfer
- **THEN** clearing the last-frame buffer and deciding to resend remain consistent so a result that clears the buffer cannot be followed by a resend of a zero-length frame for that same step
