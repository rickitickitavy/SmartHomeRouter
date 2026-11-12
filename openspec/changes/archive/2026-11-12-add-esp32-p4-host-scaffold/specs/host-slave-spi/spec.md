## ADDED Requirements

### Requirement: P4 host slave-SPI pin map is board-specific
The ESP32-P4 host image SHALL use a documented GPIO map for the inter-chip slave SPI link (`PIN_SPI_*`, `PIN_SPI_IRQ`, `PIN_SLAVE_RST`) and obligatory host status LEDs (`PIN_LED1`–`PIN_LED6`) plus `PIN_BOOT_BUTTON`. The product P4 schematic SHALL NOT require LAN, LCD, or touch. The ESP32-S3 host pin map already specified for the slave link SHALL remain valid for `esp32-s3-host`.

Locked P4 host map (product schematic): SPI SCK **12**, MOSI **11**, MISO **13**, CS **10**, IRQ **22**, slave EN/reset **9**, LED1–LED6 **2–7**, boot button **21**. GPIO **14–19** are reserved for ESP-Hosted Wi‑Fi SDIO. Future SD (if any) SHOULD prefer a separate SPI on GPIOs such as **28–31** and MUST NOT share the slave SPI controller. Avoid straps **34–38** and USB Serial/JTAG **24–25** for these product nets.

#### Scenario: S3 map unchanged
- **WHEN** the S3 host image drives the slave SPI link
- **THEN** it continues to use the existing S3 GPIO map for CS/SCK/MOSI/MISO/IRQ/RST

#### Scenario: P4 map is explicit
- **WHEN** a developer opens the P4 host pin map
- **THEN** each slave-link, LED, and boot-button pin is defined under the `BOARD_HOST_P4` section with the locked numbers above

### Requirement: Future SD SPI must not share the slave SPI controller
Future host SD-card support (out of scope for driver work in this change) MUST use a **separate** SPI controller (or bus) from the inter-chip slave SPI link. The slave C6 MUST keep its own SPI slave interface to the host. This change SHALL only document that constraint so later SD work does not multiplex onto the slave bus.

#### Scenario: Architecture note for later SD work
- **WHEN** a later change adds SD on the P4 (or S3) host
- **THEN** that change MUST assign SD to a different SPI than the slave link and MUST NOT require slave protocol changes solely to free the slave SPI pins
