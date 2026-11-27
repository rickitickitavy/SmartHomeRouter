## ADDED Requirements

### Requirement: Joined host.bin is ESP-IDF host image
For ESP32-S3 and ESP32-P4 field updates, the `host.bin` member SHALL be the ESP-IDF-built host application image for that SoC. Joined package member names (`slave.bin`, `host.bin`, version texts, optional hosted-c6 members), slave-then-host programming order, skip-when-equal rules, and “failure before host apply leaves running host unchanged” SHALL remain as in existing `joined-firmware-ota` requirements.

#### Scenario: Joined package still programs both chips
- **WHEN** an operator uploads a valid joined package built from an ESP-IDF host image and the current slave image
- **THEN** the gateway can update the slave over SPI and then apply that `host.bin` under the existing order and failure rules

#### Scenario: Package layout unchanged
- **WHEN** a developer joins host and slave binaries for Update
- **THEN** the ZIP still uses `slave.bin` and `host.bin` (plus version members) as today
