## MODIFIED Requirements

### Requirement: Same image both chips
The project SHALL produce **host** firmware application images for ESP32-S3 and ESP32-P4 and one ESP32-C6 slave image, **all built with ESP-IDF**. A developer or CI step SHALL be able to combine **one** chosen host image with the slave image into one joined Update package for the web console. Flashing SHALL NOT rely on GPIO wiring to select role. The PlatformIO default envs SHALL build the **P4 host** and the C6 slave; the **S3 host** env SHALL remain available as an explicit build (`esp32-s3-host`).

#### Scenario: Distinct binaries
- **WHEN** the S3 host, P4 host, and slave images are built
- **THEN** they are different binaries suitable only for their respective chips

#### Scenario: Default env prefers P4 host
- **WHEN** a developer runs a default PlatformIO build without selecting envs
- **THEN** the P4 host and C6 slave are the default host/slave pair, and the S3 host can still be built by selecting `esp32-s3-host`

#### Scenario: Joined package for field update
- **WHEN** an operator uses System → Update with one joined package file
- **THEN** the gateway can update both the slave and the **matching** host SoC from that single upload (package documents S3 vs P4 host)

#### Scenario: Host and slave images are ESP-IDF
- **WHEN** a developer builds `esp32-s3-host`, `esp32-p4-host`, or `esp32-c6-slave`
- **THEN** that environment uses ESP-IDF as the application framework
