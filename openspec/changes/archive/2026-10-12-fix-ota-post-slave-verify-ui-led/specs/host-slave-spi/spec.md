# Spec Delta

## ADDED Requirements

### Requirement: Status polling continues while verifying slave firmware after OTA

While the host is verifying the slave’s firmware version after a joined OTA slave transfer (not while streaming exclusive SPI OTA image frames)—including the wait after a host-initiated verify reset—the host SHALL continue to request slave status so the cached slave firmware version string can update after the slave restarts and completes bringup. Exclusive OTA-frame gating for non-OTA commands during the streaming transfer phase SHALL remain.

#### Scenario: Version refreshes during post-transfer verify

- **WHEN** the host has finished streaming the slave OTA image and is waiting to confirm the new slave version
- **THEN** the host issues status requests and updates its cached slave firmware version from status replies

#### Scenario: Version refreshes after verify reset

- **WHEN** the host has reset the slave after a failed post-transfer version wait and is waiting for ready and a fresh version read
- **THEN** the host issues status requests after bringup and updates its cached slave firmware version before deciding whether to resend

#### Scenario: Streaming phase still exclusive

- **WHEN** the host is actively streaming SPI OTA image frames to the slave
- **THEN** non-OTA SPI commands remain gated as today and MUST NOT interleave with the OTA stream

### Requirement: Post-OTA slave bringup can complete during version verify

After the slave restarts from a committed OTA image or from a host verify reset, while the host is still in post-transfer or post-reset version verification, the host SHALL allow the normal SPI bringup path (ready → settings → normal) so the slave can answer status with the running version.

#### Scenario: Slave reboots after commit during verify

- **WHEN** the slave restarts after committing the OTA image and asserts ready while the host is verifying the slave version
- **THEN** the host completes settings bringup and becomes able to read the new slave version without requiring a full gateway reboot

#### Scenario: Slave ready after verify reset

- **WHEN** the host resets the slave after a failed version wait and the slave asserts ready
- **THEN** the host completes settings bringup and can read version before any SPI resend decision
