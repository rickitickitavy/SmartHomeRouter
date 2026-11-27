## MODIFIED Requirements

### Requirement: All master–slave interactions are asynchronous
The host SHALL NOT block Wi-Fi, MQTT, the web console, CLI, or the host service loop waiting for an SPI transfer or a slave result. Callers SHALL enqueue a command (with a sequence id) and learn the outcome later via a completion or event. The slave SHALL NOT block the Zigbee stack on SPI: inbound frames SHALL be queued from the SPI path and executed later; outbound frames SHALL be queued and signalled on IRQ. A missing or late slave reply SHALL surface as an asynchronous timeout or error, not a synchronous wait.

#### Scenario: Host command from MQTT
- **WHEN** an MQTT or CLI handler needs a Zigbee action
- **THEN** it only queues the SPI command and returns, and a later host callback or event applies `CMD_RESULT`, `ERR`, or timeout

#### Scenario: Slave handles command without stalling radio
- **WHEN** the host clocks a command into the slave
- **THEN** the SPI path only queues the frame, Zigbee work runs afterward, and any reply is queued for the host to pull when IRQ is asserted

### Requirement: Host resets the slave first then brings it up
The host SHALL drive a reset line to the slave EN pin (active-low pulse) using the host GPIO assigned as `PIN_SLAVE_RST` on the ESP32-S3 pin map (design: GPIO **9**). The **first** step of host init SHALL be that pulse in **sync** mode: assert reset, wait the pulse width, and release reset before Wi-Fi, the web console, MQTT, LittleFS, or the host SPI task start. The host SHALL NOT finish that first pulse from the SPI pump task. On **every** host boot the host SHALL perform that first-step pulse. The host SHALL NOT skip it because the slave already asserts the ready line, because IRQ is already high, or because the slave still answers from a previous run. After release, the host SHALL wait for `SLAVE_READY` without blocking Wi-Fi, MQTT, web, or the host service loop. If `SLAVE_READY` does not arrive within the ready timeout, the host SHALL pulse reset again and wait again. Those later retries SHALL NOT use blocking delays.

#### Scenario: Slave ready then configure
- **WHEN** the host starts and has completed the first-step slave reset
- **THEN** it continues other host work and, when `SLAVE_READY` arrives, pushes settings and then accepts normal Zigbee operations

#### Scenario: Ready timeout
- **WHEN** the ready timeout elapses with no `SLAVE_READY`
- **THEN** the host pulses slave reset again and resumes waiting without blocking the host service loop

#### Scenario: Host boot always resets a live slave
- **WHEN** the host boots (including a warm restart) and the slave is still running from before
- **THEN** the host still performs the first-step reset pulse and does not skip it because the slave already looks ready

#### Scenario: Reset is the first host-init step
- **WHEN** the chip starts in the host role
- **THEN** the slave reset pulse is completed before Wi-Fi, the web console, MQTT, or the host SPI task start

### Requirement: Firmware-OTA retry must not send or fail on an empty last frame

While the host is streaming a slave firmware-OTA sequence (begin / chunk / end), a missing or late slave reply MAY trigger a resend of the **same** last OTA frame that was previously packed. If that last-frame buffer is empty (length zero), the host SHALL NOT enqueue a zero-length firmware-OTA command and SHALL NOT treat that empty buffer alone as grounds to abort the entire firmware-update sequence when begin has not yet been successfully staged. After begin has been staged, an empty buffer on retry SHALL either wait for the next pack from the image stream or fail with a distinct, accurate error after the allowed frame-loss budget—never invent a synthetic SPI payload.

#### Scenario: Timeout with empty buffer before begin staged

- **WHEN** the host is in the slave firmware-OTA streaming phase, begin has not been successfully staged into the last-frame buffer, and a timeout or NACK retry path runs
- **THEN** the host does not enqueue a firmware-OTA frame with length zero and does not abort solely with “Firmware OTA frame missing”

#### Scenario: Resend uses the packed last frame

- **WHEN** a slave firmware-OTA frame was packed into the last-frame buffer and a missing or late reply triggers a resend within the allowed fail budget
- **THEN** the host re-enqueues that same packed payload and sequence as today

#### Scenario: Concurrent pump and SPI event agree on buffer lifetime

- **WHEN** the host service loop pumps firmware-OTA progress while the host SPI task delivers OTA command results or timeouts for the same transfer
- **THEN** clearing the last-frame buffer and deciding to resend remain consistent so a result that clears the buffer cannot be followed by a resend of a zero-length frame for that same step
