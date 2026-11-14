# Spec Delta

## Purpose

Recovers a wedged host when internal heap is critically low by stopping work, freeing the AsyncWebServer, flushing history with the existing snapshot path, and rebooting so the router can become reachable again. Operators set the trip threshold in System → Hardware.

## ADDED Requirements

### Requirement: Hardware Low memory threshold setting
System → Hardware SHALL show **Low memory threshold** as an integer field in **kB** from **10** to **32** (factory default **16**). GET `/api/hardware` SHALL return `lowMemoryThresholdKb` (or an equivalent documented key). POST `/api/hardware` SHALL accept it, reject values outside 10–32, and persist it with hardware settings. Omitted on older clients MUST keep the prior stored value or factory default. Only `isAdmin` SHALL use Hardware. The field SHALL apply on both ESP32-S3 and ESP32-P4 host images. Global settings export and restore SHALL include this value with the other hardware settings.

#### Scenario: Default threshold
- **WHEN** hardware settings have never stored a low-memory threshold
- **THEN** the effective threshold is 16 kB

#### Scenario: Reject out of range
- **WHEN** an admin POSTs `lowMemoryThresholdKb` below 10 or above 32
- **THEN** the host rejects the request and does not change the stored threshold

#### Scenario: Export includes threshold
- **WHEN** an admin exports settings
- **THEN** the export payload includes the stored low-memory threshold with hardware settings

### Requirement: Trip when free internal heap is at or below the configured threshold
On the host (ESP32-S3 and ESP32-P4 images), the firmware SHALL monitor free **internal** heap. When free internal heap is less than or equal to **`lowMemoryThresholdKb` × 1024** bytes, the host SHALL enter the low-memory fail-safe sequence. The slave SHALL NOT implement this trip. The trip SHALL latch so the sequence runs at most once until the next boot.

#### Scenario: Heap at threshold trips
- **WHEN** host free internal heap reports at or below the configured threshold in bytes and the fail-safe has not yet latched this boot
- **THEN** the host starts the low-memory fail-safe sequence

#### Scenario: Above threshold does not trip
- **WHEN** host free internal heap stays above the configured threshold in bytes
- **THEN** the host does not enter the low-memory fail-safe sequence for that reading

#### Scenario: Changing threshold applies without rebuild
- **WHEN** an admin saves a new in-range Low memory threshold
- **THEN** subsequent fail-safe checks use that new threshold (no firmware reflash required)

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
