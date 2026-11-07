## ADDED Requirements

### Requirement: Slave SPI begin must not abort on empty last-frame buffer

When a joined package includes `slave.bin`, after staging and package prepare succeed the host SHALL start the slave SPI transfer cycle and SHALL successfully queue the firmware-OTA begin frame (or keep retrying queue-full without failing) before it may report a hard joined-update failure for a missing last-frame buffer. While begin has not yet been successfully queued for SPI, an empty last-frame buffer SHALL NOT abort the joined update with a “Firmware OTA frame missing” (or equivalent) hard error. Host-only packages (no `slave.bin`) are unchanged.

#### Scenario: Joined package with slave reaches begin queue

- **WHEN** the operator uploads a valid joined ZIP that contains `slave.bin` and staging/prepare succeed
- **THEN** the host enters the slave SPI transfer cycle and queues the OTA begin frame without failing solely because the last-frame buffer length was temporarily zero

#### Scenario: Empty buffer before begin is not a hard fail

- **WHEN** the host is in the slave SPI transfer cycle and begin has not yet been successfully queued, and a retry path observes an empty last-frame buffer
- **THEN** the host does not mark the joined update Failed for “frame missing” and either waits to pack begin or continues the normal begin pack/send path

#### Scenario: Host-only package unaffected

- **WHEN** the joined ZIP has no `slave.bin` and only `host.bin`
- **THEN** the host still skips slave SPI and applies the host image as today
