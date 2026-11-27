## Purpose

Defines that ESP32-S3 host, ESP32-P4 host, and ESP32-C6 slave firmware build and run as pure ESP-IDF while remaining wire- and behavior-compatible with all existing host-facing protocols, SPI/Zigbee contracts, and product logic.

## ADDED Requirements

### Requirement: Host and slave build as pure ESP-IDF
The ESP32-S3 host, ESP32-P4 host, and ESP32-C6 slave PlatformIO environments SHALL build with ESP-IDF as the application framework (not the Arduino framework). Role-by-image rules from `board-role` SHALL remain.

#### Scenario: S3 host is IDF
- **WHEN** a developer builds `esp32-s3-host`
- **THEN** the resulting application image is an ESP-IDF host binary suitable for the S3 chip

#### Scenario: P4 host is IDF
- **WHEN** a developer builds `esp32-p4-host`
- **THEN** the resulting application image is an ESP-IDF host binary suitable for the P4 chip

#### Scenario: C6 slave is IDF
- **WHEN** a developer builds `esp32-c6-slave`
- **THEN** the resulting application image is an ESP-IDF slave binary suitable for the C6 chip

### Requirement: HTTP and network logic compatibility
After the host ESP-IDF migration, operator-visible HTTP routes and JSON shapes, Wi‑Fi AP/STA/SoftAP recovery, and MQTT broker/client behavior SHALL remain compatible with the pre-migration host.

#### Scenario: HTTP console still serves the same APIs
- **WHEN** a signed-in operator uses the web console against an ESP-IDF host
- **THEN** existing documented `/api/*` routes and field sets the console depends on still work without a console redesign

### Requirement: SPI and update logic compatibility
Host↔slave SPI framing (including firmware-OTA begin/chunk/end), joined ZIP layout, and slave-then-host update order SHALL remain compatible with the pre-migration stack. An ESP-IDF host MUST still interoperate with a pre-migration Arduino C6 slave image for emergency/rollback paths; the shipped cutover SHALL pair IDF host with IDF slave.

#### Scenario: Pre-migration slave still speaks SPI
- **WHEN** an ESP-IDF host runs against an unchanged pre-migration Arduino C6 slave image
- **THEN** SPI commands, events, settings push, and slave firmware-OTA framing continue to interoperate

#### Scenario: IDF slave speaks the same SPI
- **WHEN** an ESP-IDF host runs against an ESP-IDF C6 slave built from this change
- **THEN** SPI commands, events, settings push, Zigbee-related events, and slave firmware-OTA framing continue to interoperate

### Requirement: Product feature logic compatibility
SMTP configured-gate and async send semantics, STAR interconnect pairing/exchange rules, persisted stores, automations, consoles, scenes, cameras, history, and settings export/restore SHALL remain compatible. This change SHALL NOT redefine product trigger, condition, or automation semantics.

#### Scenario: Settings export still works
- **WHEN** an admin exports settings from an ESP-IDF host
- **THEN** persisted feature groups remain compatible with the pre-migration export (Wi‑Fi credentials still excluded)

### Requirement: Dual-host and slave parity for the migration
The ESP-IDF migration SHALL land on both S3 and P4 host builds and the C6 slave in the same change unless a later change explicitly scopes otherwise. Board-specific HAL (native Wi‑Fi vs ESP-Hosted, flash/SD, pins, Zigbee radio) MAY differ; protocol and product semantics MUST NOT silently diverge.

#### Scenario: Host and slave envs migrate together
- **WHEN** this capability is implemented
- **THEN** `esp32-s3-host`, `esp32-p4-host`, and `esp32-c6-slave` build and run as ESP-IDF images with the same product semantics as before the migration

### Requirement: Boundary — STA web rebind
ESP-IDF hosts MUST still rebind the HTTP listen after STA/Hosted address recovery from the host service loop only, and port 80 MUST answer after recovery.

#### Scenario: Rebind after STA recovery
- **WHEN** STA or P4 Hosted recovery changes the usable address
- **THEN** the console becomes reachable again on port 80 without requiring a device reboot

### Requirement: Boundary — SPI async and first-step slave reset
ESP-IDF hosts MUST still pulse slave reset as the first host-init step before Wi‑Fi/web/MQTT/SPI-task bring-up, wait for `SLAVE_READY` without blocking the host service loop, and keep all master–slave SPI interactions asynchronous.

#### Scenario: Reset before network stack
- **WHEN** an ESP-IDF host boots
- **THEN** the slave reset pulse completes before Wi‑Fi, the web console, MQTT, or the host SPI task start

### Requirement: Boundary — joined update order
ESP-IDF hosts MUST still accept the joined ZIP (`slave.bin` + `host.bin`, optional hosted-c6 members), program the slave over SPI first when needed, verify as today, then apply `host.bin` when needed. Slave SPI OTA begin/chunk/end, ACK/retry/timeout, and exclusive transfer rules SHALL remain. Failure before host apply MUST leave the running host application unchanged.

#### Scenario: Slave then host
- **WHEN** a joined Update needs both images
- **THEN** the ESP-IDF host completes slave SPI programming (and verify rules) before applying `host.bin`

#### Scenario: Failed slave transfer leaves host app
- **WHEN** slave SPI transfer or commit fails during joined update on an ESP-IDF host
- **THEN** the running host application remains unchanged

### Requirement: Boundary — Zigbee slave start
ESP-IDF slaves MUST still start Zigbee after host settings (including while the host is in Wi‑Fi AP or STA), pause the SPI pump during radio start as today, and surface join/rebind/attr events over the same SPI event path.

#### Scenario: Settings start Zigbee
- **WHEN** the host pushes Zigbee settings to an ESP-IDF slave that has not started the coordinator
- **THEN** the slave starts Zigbee on its start task and reports started state over SPI as before
