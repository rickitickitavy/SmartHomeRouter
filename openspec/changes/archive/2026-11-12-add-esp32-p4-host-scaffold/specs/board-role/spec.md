## MODIFIED Requirements

### Requirement: Role pin sampled at boot
The product SHALL NOT sample a GPIO role strap to choose host vs slave. An **ESP32-S3 host** firmware image and an **ESP32-P4 host** firmware image SHALL each always run as the Wi-Fi/SPI host when flashed to that chip. The ESP32-C6 firmware image SHALL always run as the Zigbee/SPI slave. Role SHALL be fixed by which image is flashed to which chip.

#### Scenario: Host image on S3
- **WHEN** the ESP32-S3 host image boots
- **THEN** the chip runs as host (Wi-Fi, settings, web, MQTT, SPI master) and does not start the Zigbee radio

#### Scenario: Host image on P4
- **WHEN** the ESP32-P4 host image boots
- **THEN** the chip runs as host (Wi-Fi, settings, web, MQTT, SPI master) and does not start the Zigbee radio

#### Scenario: Slave image on C6
- **WHEN** the ESP32-C6 slave image boots
- **THEN** the chip runs as slave (Zigbee radio and SPI slave only) and does not start Wi-Fi, the web console, or MQTT

### Requirement: Same image both chips
The project SHALL produce **host** firmware application images for ESP32-S3 and ESP32-P4, plus one ESP32-C6 slave image. A developer or CI step SHALL be able to combine **one** chosen host image with the slave image into one joined Update package for the web console. Flashing SHALL NOT rely on GPIO wiring to select role. The PlatformIO default envs SHALL build the **P4 host** and the C6 slave; the **S3 host** env SHALL remain available as an explicit build (`esp32-s3-host`).

#### Scenario: Distinct binaries
- **WHEN** the S3 host, P4 host, and slave images are built
- **THEN** they are different binaries suitable only for their respective chips

#### Scenario: Default env prefers P4 host
- **WHEN** a developer runs a default PlatformIO build without selecting envs
- **THEN** the P4 host and C6 slave are the default host/slave pair, and the S3 host can still be built by selecting `esp32-s3-host`

#### Scenario: Joined package for field update
- **WHEN** an operator uses System → Update with one joined package file
- **THEN** the gateway can update both the slave and the **matching** host SoC from that single upload (package documents S3 vs P4 host)

### Requirement: README documents S3 host and C6 slave roles
The project `README.md` MUST describe the product as distinct firmware images: ESP32-S3 Wi‑Fi/SPI host, ESP32-P4 Wi‑Fi/SPI host, and ESP32-C6 Zigbee/SPI slave, with role fixed by which image is flashed. It MUST document how to build the legacy S3 host env and the default P4 host env. It MUST NOT instruct operators to select host vs slave with a GPIO strap or to flash one shared binary to both host and slave chips.

#### Scenario: README role model matches firmware
- **WHEN** a developer opens `README.md` at the repo root
- **THEN** the hardware and role sections name ESP32-S3 host, ESP32-P4 host, and ESP32-C6 slave and do not require a GPIO strap for role

#### Scenario: README does not claim one binary for both chips
- **WHEN** a developer follows the flash instructions in `README.md`
- **THEN** those instructions describe distinct host and slave images (or a joined package built from a chosen host + slave), not one `.bin` for host and slave together as a single SoC image

## ADDED Requirements

### Requirement: Dual-host feature parity by default
When a change adds or alters **host** product behavior (web console, settings, MQTT, automations, cameras UI, stores, SPI master features, and similar), that behavior MUST be implemented so both the ESP32-S3 host build and the ESP32-P4 host build provide it, unless the change **explicitly** scopes the work to one host SoC. Pin maps, flash size, USB CDC, and HAL stubs MAY differ by board; product semantics MUST NOT silently diverge.

#### Scenario: Unscoped host feature lands on both envs
- **WHEN** a change adds a host-side feature without naming S3-only or P4-only scope
- **THEN** both `esp32-s3-host` and `esp32-p4-host` build that feature

#### Scenario: Explicit single-host scope is allowed when stated
- **WHEN** a change explicitly states it is P4-only (or S3-only)
- **THEN** the other host env is not required to gain that feature in the same change

### Requirement: Host firmware version SoC prefix
The host firmware identity string SHALL be `S3-MAJOR.MINOR.BUILD` when built for ESP32-S3 and `P4-MAJOR.MINOR.BUILD` when built for ESP32-P4. The numeric core SHALL remain `MAJOR.MINOR.BUILD` shared across host SoCs for a given release train. The slave identity string SHALL remain unprefixed `MAJOR.MINOR.BUILD`. Status UI, boot logs, and joined-package `host.version.txt` SHALL use the prefixed host string for that image.

#### Scenario: S3 host reports prefixed version
- **WHEN** the ESP32-S3 host image runs with core `4.2.22`
- **THEN** the host identifies itself as `S3-4.2.22`

#### Scenario: P4 host reports prefixed version
- **WHEN** the ESP32-P4 host image runs with core `4.2.22`
- **THEN** the host identifies itself as `P4-4.2.22`
