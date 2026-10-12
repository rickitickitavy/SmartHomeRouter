# Spec Delta

## MODIFIED Requirements

### Requirement: RGB and LED1–LED4 work together

Each chip SHALL drive LED1–LED6 on that board’s pin map (host ESP32-S3 and slave ESP32-C6 as in the Host status requirement / pins). Host LED3 SHALL stay off. Host LED4 SHALL stay on while the local MQTT broker is listening (after boot). Host LED5 SHALL mean MQTT connected (solid) or boot (blink) and SHALL NOT mean any error. Host LED6 SHALL mean critical error (solid) or firmware update cycle (5 Hz blink). Slave LED5 SHALL mean ready with no fault (solid) or boot/critical (blink). Slave LED6 SHALL mean pairing blink. Red LEDs SHALL light with digital HIGH when on. Blue and green LEDs SHALL light with PWM when on. The onboard WS2812 SHALL NOT be used.

#### Scenario: Pins

- **WHEN** status indication is shown
- **THEN** host uses LED1, LED2, LED4, LED5, and LED6; slave uses LED1–LED6

#### Scenario: Drive by color

- **WHEN** a status LED is logically on
- **THEN** a red LED is digital HIGH and a blue or green LED is PWM at that color’s brightness

### Requirement: Host LED6 blinks during firmware update cycle

While the host firmware update cycle is active (HTTP receive, slave SPI OTA, host apply, or reboot-armed after a successful update) and no critical error is present, the host SHALL blink **LED6** at 5 Hz (100 ms on, 100 ms off). During the slave SPI image-transfer phase, the host SHALL turn off the normal continuous update-blink mechanism and SHALL toggle LED6 manually between SPI packets at the same 5 Hz cadence so the blink remains visible while packets are in flight. After that SPI transfer phase ends, the host MAY restore the normal update-blink mechanism for the rest of the cycle. When a critical error is present, LED6 SHALL stay solid for critical and SHALL NOT show the update blink. Host LED5 SHALL NOT indicate update progress.

#### Scenario: Update cycle running

- **WHEN** the host is applying a firmware update cycle and no critical error is present
- **THEN** host LED6 blinks at 5 Hz

#### Scenario: Slave SPI transfer keeps LED6 blinking

- **WHEN** the host is transferring slave firmware over SPI with no critical error present
- **THEN** host LED6 continues to blink at 5 Hz between SPI packets

#### Scenario: Update cycle with critical

- **WHEN** a critical error is present during a firmware update cycle
- **THEN** host LED6 stays solid for critical and does not blink for update

#### Scenario: Update cycle finished

- **WHEN** the firmware update cycle is no longer active and no critical error is present
- **THEN** host LED6 is off
