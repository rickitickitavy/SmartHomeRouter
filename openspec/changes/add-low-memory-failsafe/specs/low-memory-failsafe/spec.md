# Spec Delta

## Purpose

Recovers a wedged host when internal heap is critically low by stopping work, freeing the AsyncWebServer, flushing history with the existing snapshot path, and rebooting so the router can become reachable again.

## ADDED Requirements

### Requirement: Trip when free internal heap is at or below 15 kB
On the host (ESP32-S3 and ESP32-P4 images), the firmware SHALL monitor free **internal** heap. When free internal heap is less than or equal to **15360** bytes (15 × 1024), the host SHALL enter the low-memory fail-safe sequence. The slave SHALL NOT implement this trip. The trip SHALL latch so the sequence runs at most once until the next boot.

#### Scenario: Heap at threshold trips
- **WHEN** host free internal heap reports 15360 bytes or less and the fail-safe has not yet latched this boot
- **THEN** the host starts the low-memory fail-safe sequence

#### Scenario: Above threshold does not trip
- **WHEN** host free internal heap stays above 15360 bytes
- **THEN** the host does not enter the low-memory fail-safe sequence for that reading

### Requirement: Fail-safe stops work then frees AsyncWebServer
After the trip latches, the host SHALL stop ongoing application serving/work as needed to avoid further heap growth, then SHALL stop, destroy, and free the AsyncWebServer so its listen sockets, handlers’ connection state, and related buffers are released. After this step the HTTP console NEED NOT remain reachable until reboot completes.

#### Scenario: Web server torn down
- **WHEN** the low-memory fail-safe runs
- **THEN** the AsyncWebServer is stopped/destroyed and its memory is freed before the history save step completes

### Requirement: Fail-safe saves history then reboots
After AsyncWebServer teardown, the host SHALL save historical data using the same snapshot algorithm used for a normal pre-restart history flush (current backend: LittleFS or SD as already selected by product logic). The host SHALL then reboot. Failure to save history MUST NOT skip the reboot indefinitely; the host MAY log and still reboot.

#### Scenario: History then reboot
- **WHEN** the fail-safe has torn down the web server and history samples exist
- **THEN** the host attempts the current history snapshot persist and then reboots

#### Scenario: Persist failure still reboots
- **WHEN** the history snapshot persist fails or is skipped during the fail-safe
- **THEN** the host still reboots
