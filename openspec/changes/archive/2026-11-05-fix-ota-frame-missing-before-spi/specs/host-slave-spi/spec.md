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

### Requirement: Only one in-flight slave OTA frame at a time

While streaming slave firmware OTA, the host SHALL have at most one firmware-OTA command awaiting a slave reply. The host MUST NOT enqueue a new OTA sequence id while a previous OTA command is still pending a result. Success and failure results (and local timeouts) SHALL be applied only when their sequence id matches the in-flight OTA frame; mismatched late results SHALL be ignored without advancing the image byte offset.

#### Scenario: No second OTA seq while reply pending

- **WHEN** a slave firmware-OTA frame has been sent and the host is still waiting for its command result
- **THEN** the host does not enqueue another firmware-OTA frame with a different sequence id

#### Scenario: Late ACK for previous seq is ignored

- **WHEN** the host is waiting for sequence N and a command-result for sequence N−1 (or any other seq) arrives
- **THEN** the host ignores that result and does not advance the slave image offset

#### Scenario: Matching ACK advances offset

- **WHEN** the host is waiting for sequence N and a successful command-result for sequence N arrives
- **THEN** the host advances the image offset by that frame’s data length and may enqueue the next OTA frame

### Requirement: END NACK must not be treated as complete-image success

If the slave negatively acknowledges a firmware-OTA END frame, the host SHALL NOT treat the transfer as complete and SHALL NOT enter post-transfer version verify solely because the fail budget was exhausted on those NACKs. A verify-after-END-timeout escape MAY apply only when the losses were reply timeouts on an END frame after prior bytes were accounted for—not when the slave explicitly rejected END.

#### Scenario: Repeated END NACK fails the update

- **WHEN** the slave returns failure for the END frame until the host’s frame-loss budget is exhausted
- **THEN** the joined slave update fails with an error and the host does not claim “END ACK lost after complete image”

#### Scenario: END timeout after full image may verify

- **WHEN** the in-flight frame is END, prior image bytes are complete, and the host exhausts the fail budget on reply timeouts (not NACKs)
- **THEN** the host MAY proceed to slave version verification as today
