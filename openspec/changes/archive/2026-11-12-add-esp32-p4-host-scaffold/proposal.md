## Why

The product needs a path to an **ESP32-P4 Wi‑Fi 6** host while keeping the existing **ESP32-S3** host buildable. Operators and CI must not lose S3; developers must be able to default to P4. Mic, speaker, camera, SD-on-second-SPI, and USB-OTG are **future** hardware — this change only scaffolds dual-host builds so later features can land on both hosts unless a change explicitly scopes one board.

## What Changes

- Add PlatformIO env `esp32-p4-host` targeting a **generic Espressif P4 Wi‑Fi 6 kit** (exact board id / pins stubbed TBD where Espressif Arduino board support requires placeholders).
- Change `default_envs` so the **default host is P4** plus the existing C6 slave; **`esp32-s3-host` remains a first-class env** that must still compile and flash.
- Host role model becomes: flash selects **S3 host image** or **P4 host image** (still not a GPIO strap); slave remains ESP32-C6 Zigbee/SPI only.
- Introduce a standing product rule: **new host features MUST work on both S3 and P4 hosts** unless a change explicitly scopes one board.
- Document intended **future** H/W (mic, speaker, camera if possible, SD on a SPI bus **separate** from the slave link, USB OTG if possible) without implementing drivers in this change.
- Slave SPI protocol and C6 env are **unchanged** in this change.
- Host firmware identity strings are SoC-prefixed: **`S3-MAJOR.MINOR.BUILD`** (e.g. `S3-4.2.22`) and **`P4-MAJOR.MINOR.BUILD`**. Slave stays unprefixed `MAJOR.MINOR.BUILD`. Status, logs, and `host.version.txt` MUST use the prefixed host string for the image that was built.
- Bump `FIRMWARE_VERSION_HOST` build only (no slave source in this change).

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `board-role`: allow ESP32-P4 as an alternate host image alongside ESP32-S3; dual-host feature parity rule; README/build docs
- `host-slave-spi`: P4 host pin map stubs for the existing slave SPI link (TBD GPIOs on generic kit); document that a future SD bus MUST NOT share that SPI controller

## Impact

`platformio.ini` (new env + default_envs), host pin headers / board flags, README flash/build notes, possibly partition/sdkconfig stubs for P4. No mic/speaker/camera/SD/OTG drivers. No slave SPI protocol or C6 env changes. Joined OTA packaging continues to pair **one host image + slave**; CI/docs MUST state which host image is in a given joined package.
