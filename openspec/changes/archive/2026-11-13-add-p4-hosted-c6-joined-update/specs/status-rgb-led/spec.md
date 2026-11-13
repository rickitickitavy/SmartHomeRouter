## MODIFIED Requirements

### Requirement: RGB and LED1–LED4 work together

Each chip SHALL drive LED1–LED6 on that board’s pin map (host ESP32-S3 / ESP32-P4 and slave ESP32-C6 as in the Host status requirement / pins). Host LED3 SHALL indicate web console activity (on while any HTTP or WebSocket request is in progress; off when that response is finished). Host LED4 SHALL stay on while the local MQTT broker is listening (after boot). Host LED5 SHALL mean MQTT connected (solid) or boot (blink) and SHALL NOT mean any error. Host LED6 SHALL mean critical error (solid) or firmware update cycle (5 Hz blink). Slave LED5 SHALL mean ready with no fault (solid) or boot/critical (blink). Slave LED6 SHALL mean pairing blink. Red LEDs SHALL light with digital HIGH when on. Blue and green LEDs SHALL light with PWM when on. The onboard WS2812 SHALL NOT be used.

#### Scenario: Pins

- **WHEN** status indication is shown
- **THEN** host uses LED1–LED6; slave uses LED1–LED6

#### Scenario: Host web request activity

- **WHEN** the host receives any web HTTP request or WebSocket client frame (including upgrade)
- **THEN** host LED3 turns on for that in-flight work and turns off when the matching response is finished

### Requirement: Fixed LED colors per board role

Each board SHALL treat LED1–LED6 as fixed physical colors by role. Host (master): LED1 blue, LED2 green, LED3 blue, LED4 green, LED5 green, LED6 red. Slave: LED1 red, LED2 green, LED3 green, LED4 blue, LED5 green, LED6 blue. Status meanings for each LED index SHALL stay as specified; only the drive method and brightness by color change.

#### Scenario: Host color map

- **WHEN** the host drives LED1–LED6 for status
- **THEN** LED1 is blue, LED2 green, LED3 blue, LED4–LED5 green, and LED6 red

### Requirement: Host RGB green while MQTT is connected

After host boot indication is cleared, the host SHALL hold **LED5** on while MQTT is connected, whether SERVER TYPE is `remote` or `local`. LED5 SHALL not stay on while SERVER TYPE is `disable`, or while MQTT is not connected. Host LED3 SHALL be reserved for web request activity and SHALL NOT mean MQTT connection.

#### Scenario: Remote broker connected

- **WHEN** the host is ready, SERVER TYPE is `remote`, and MQTT is connected with no web request in flight
- **THEN** host LED5 is on and host LED3 is off

### Requirement: Host LED4 on while the local MQTT broker is listening

After host boot indication is cleared, host LED4 SHALL stay **on** while the onboard MQTT broker is listening, including while a critical error is indicated on LED6. Host LED3 SHALL NOT mean local-broker listening. The onboard RGB SHALL NOT be used for the local broker.

### Requirement: Host pulses LED1 and LED2 for MQTT device traffic

After host boot indication is cleared, the host SHALL pulse LED1 for 0.1 seconds when it receives a device command from MQTT, and SHALL pulse LED2 for 0.1 seconds when it successfully publishes a device state message. Host LED3 SHALL NOT pulse for MQTT traffic (it is for web request activity). Host LED4 SHALL stay on while the local MQTT broker is listening and SHALL NOT pulse for MQTT traffic. LED5 SHALL NOT flash for those MQTT events.
