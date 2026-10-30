# Spec Delta

## MODIFIED Requirements

### Requirement: Hardware tab sets blue and green LED brightness

System → Hardware SHALL show SPI speed plus **BLUE LED brightness** and **GREEN LED brightness** as integer fields from 1 to 100 (percent), and a checkbox **use queue for base device command** (square-only toggle; default unchecked / false). GET `/api/hardware` SHALL return `spiSpeedHz`, `blueLedBrightness`, `greenLedBrightness`, and `useQueueForBaseDeviceCommand` (boolean). POST `/api/hardware` SHALL require the brightness and SPI fields as today, accept the boolean (omitted on older clients MUST mean false), reject values outside 1–100 for either brightness, persist them with hardware settings, apply brightness without requiring a reboot, push brightness to the slave when the link is up, and persist the queue flag for use by the action/command FIFO. Factory default for both brightness fields SHALL be 100. Factory default for **use queue for base device command** SHALL be false. Only `isAdmin` SHALL use Hardware.

#### Scenario: Load shows brightness

- **WHEN** an admin opens System → Hardware
- **THEN** blue and green brightness show the stored values (default 100 if never saved) and the queue checkbox reflects the stored flag (default unchecked)

#### Scenario: Save applies brightness

- **WHEN** the admin sets blue to 40 and green to 70 and clicks Save
- **THEN** GET `/api/hardware` returns those values and host blue/green LEDs use them while on

#### Scenario: Out of range rejected

- **WHEN** POST `/api/hardware` sends `blueLedBrightness` 0 or 101
- **THEN** the host responds 400 and does not change stored brightness

#### Scenario: Save queue flag

- **WHEN** the admin checks **use queue for base device command** and saves
- **THEN** GET `/api/hardware` returns `useQueueForBaseDeviceCommand` true and base UI device commands use the action/command FIFO

### Requirement: Status Memory shows free RAM and PSRAM

The Status Memory card MUST be titled to reflect free memory (for example **Free memory** or group title **Memory** with row labels **Free RAM** / **Free PSRAM**). Values MUST show free heap/PSRAM in kB and free percent of total (not used amounts). When PSRAM is absent, PSRAM MUST show as unavailable (em dash or equivalent), not zero used. The Free memory group MUST also show the action/command FIFO busy count (pending element count) from gateway status. Operator Status and simple-user Status MUST both show that busy count.

#### Scenario: Free heap displayed

- **WHEN** the operator opens Status and the host reports free heap
- **THEN** the Memory card shows free RAM in kB and free percent, not used heap

#### Scenario: Free PSRAM when present

- **WHEN** PSRAM is present
- **THEN** the Memory card shows free PSRAM in kB and free percent

#### Scenario: Queue busy count in Free memory

- **WHEN** Status is open and three elements are pending in the action/command FIFO
- **THEN** Free memory shows busy count 3 on the operator page and on the simple-user Status view

## ADDED Requirements

### Requirement: Base UI device commands honor hardware queue flag

POST `/api/devices/command` (Manual command, Devices row type actions, Place and simple-user widget controls, and any other base live control using that path) MUST enqueue on the action/command FIFO when **use queue for base device command** is true, for every device kind that path supports. When the flag is false, that path MUST keep immediate execution. Devices Configure (`/api/devices/configure`) MUST remain outside this queue.

#### Scenario: Flag on queues Manual command

- **WHEN** the flag is true and the operator sends `ON` via Manual command
- **THEN** the body is enqueued and applied only when the main-loop FIFO drain runs it

#### Scenario: Flag off keeps Devices row action immediate

- **WHEN** the flag is false and the operator chooses OFF from a Devices row type-action menu
- **THEN** the host applies OFF on the immediate device-command path
