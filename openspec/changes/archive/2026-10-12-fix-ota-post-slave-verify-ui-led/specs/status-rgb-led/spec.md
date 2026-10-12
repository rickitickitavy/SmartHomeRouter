# Spec Delta

## MODIFIED Requirements

### Requirement: Host LED6 blinks during firmware update cycle

While the host firmware update cycle is active (HTTP receive, slave SPI OTA, host apply, or reboot-armed after a successful update) and no critical error is present, the host SHALL blink **LED6** at 10 Hz (50 ms on, 50 ms off). During the slave SPI image-transfer phase, the host SHALL turn off the normal continuous update-blink mechanism and SHALL toggle LED6 manually between SPI packets at the same 10 Hz cadence so the blink remains visible while packets are in flight. After that SPI transfer phase ends, the host MAY restore the normal update-blink mechanism for the rest of the cycle. When a critical error is present, LED6 SHALL stay solid for critical and SHALL NOT show the update blink. Host LED5 SHALL NOT indicate update progress.

#### Scenario: Update cycle running

- **WHEN** the host is applying a firmware update cycle and no critical error is present
- **THEN** host LED6 blinks at 10 Hz

#### Scenario: Slave SPI transfer keeps LED6 blinking

- **WHEN** the host is transferring slave firmware over SPI with no critical error present
- **THEN** host LED6 continues to blink at 10 Hz between SPI packets

#### Scenario: Update cycle with critical

- **WHEN** a critical error is present during a firmware update cycle
- **THEN** host LED6 stays solid for critical and does not blink for update

#### Scenario: Update cycle finished

- **WHEN** the firmware update cycle is no longer active and no critical error is present
- **THEN** host LED6 is off
