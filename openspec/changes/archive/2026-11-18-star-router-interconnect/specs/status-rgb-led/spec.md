# Spec Delta

## ADDED Requirements

### Requirement: Host LED6 blinks during interconnect pairing wait
While a Repeater interconnect pairing wait is active (Pair to host listen window) and no critical error and no firmware-update cycle is present, the host SHALL blink **LED6** (red) at **2 Hz** (250 ms on, 250 ms off). Critical solid LED6 and firmware-update 5 Hz blink SHALL outrank this interconnect pairing blink. When the pairing wait ends (success, cancel, or timeout), the host SHALL stop the interconnect pairing blink.

#### Scenario: Pairing wait blinks red at 2 Hz
- **WHEN** Repeater Pair to host wait is active and no critical error or firmware update is present
- **THEN** host LED6 blinks at 2 Hz

#### Scenario: Critical outranks interconnect pairing blink
- **WHEN** a critical error is present during interconnect pairing wait
- **THEN** host LED6 stays solid for critical and does not show the 2 Hz pairing blink

#### Scenario: Pairing wait ended
- **WHEN** interconnect pairing wait ends and no critical error or firmware update is present
- **THEN** host LED6 is not blinking for interconnect pairing

## MODIFIED Requirements

### Requirement: RGB and LED1–LED4 work together

Each chip SHALL drive LED1–LED6 on that board’s pin map (host ESP32-S3 / ESP32-P4 and slave ESP32-C6 as in the Host status requirement / pins). Host LED3 SHALL indicate web console activity (on while any HTTP or WebSocket request is in progress; off when that response is finished). Host LED4 SHALL stay on while the local MQTT broker is listening (after boot). Host LED5 SHALL mean MQTT connected (solid) or boot (blink) and SHALL NOT mean any error. Host LED6 SHALL mean critical error (solid), firmware update cycle (5 Hz blink), or Repeater interconnect pairing wait (2 Hz blink). Slave LED5 SHALL mean ready with no fault (solid) or boot/critical (blink). Slave LED6 SHALL mean pairing blink. Red LEDs SHALL light with digital HIGH when on. Blue and green LEDs SHALL light with PWM when on. The onboard WS2812 SHALL NOT be used.

#### Scenario: Pins

- **WHEN** status indication is shown
- **THEN** host uses LED1–LED6; slave uses LED1–LED6

#### Scenario: Drive by color

- **WHEN** a status LED is logically on
- **THEN** a red LED is digital HIGH and a blue or green LED is PWM at that color’s brightness

#### Scenario: Host web request activity

- **WHEN** the host receives any web HTTP request or WebSocket client frame (including upgrade)
- **THEN** host LED3 turns on for that in-flight work and turns off when the matching response is finished
