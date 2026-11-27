# Proposal

## Why

Host firmware (ESP32-S3 and ESP32-P4) and the ESP32-C6 slave still run on the Arduino framework layered on ESP-IDF. That layer costs scarce internal DRAM, obscures task/WDT control, and fights with fragile paths (HTTP concurrency, SMTP/TLS core pinning, joined OTA apply, Zigbee + SPI slave). Operators need the same product behavior with pure ESP-IDF on **host and slave** so RAM and scheduling can be controlled without changing protocols or logic — and so the cutover is one coherent join, not a half-migrated stack.

## What Changes

- Migrate **both** host builds (`esp32-s3-host`, `esp32-p4-host`) **and** the C6 slave (`esp32-c6-slave`) to **full ESP-IDF** (PlatformIO `framework = espidf`, no Arduino application framework on any role).
- Land host and slave IDF images in the **same change** so joined Update stays a single cutover (no required “Arduino slave forever” phase). SPI wire format stays compatible with the pre-migration Arduino slave for field path safety.
- Replace Arduino-centric libraries with IDF equivalents while **preserving** all external protocols and product logic: HTTP/API/JSON, Wi‑Fi AP/STA/SoftAP recovery, MQTT broker/client, host↔slave SPI (including OTA begin/chunk/end), joined ZIP update order, SMTP enqueue semantics, interconnect AEAD/TCP, Zigbee coordinator behavior, stores, automations, consoles, scenes, cameras, history, settings export/restore, Status metrics.
- Retarget boot from `setup`/`loop` to `app_main` + FreeRTOS tasks without changing observable scheduling intent (SPI, web, MQTT, SMTP core placement, Zigbee start task).
- Update specs that hard-code Arduino/`AsyncWebServer` wording to framework-neutral language that keeps the same requirements.
- Host firmware version for this change track: **5.0.10+** (`S3-…` / `P4-…`). Slave version bumps with slave source (**4.2.20+**).
- Fragile zones **approved** for this change: `smtp-send`, `firmware-update-process`, `slave-spi-ota`.

## Capabilities

### New Capabilities
- `host-esp-idf`: Host and slave builds run as pure ESP-IDF on S3/P4/C6; all host-facing and SPI/Zigbee protocols and product logic stay wire- and behavior-compatible, including documented boundary cases.

### Modified Capabilities
- `host-slave-spi`: Replace host “Arduino `loop`” wording with the host service/scheduler loop; async SPI semantics unchanged; slave SPI/OTA apply uses IDF flash/OTA APIs with the same framing.
- `low-memory-failsafe`: Teardown the host HTTP console server (not specifically AsyncWebServer) under the same fail-safe sequence.
- `control-consoles`: Heap-reserve wording refers to the HTTP console + Wi‑Fi stack, not AsyncWebServer by name.
- `web-console`: Framework-neutral host wording where Arduino/AsyncTCP are named as implementation; operator-visible HTTP/JSON behavior unchanged.
- `onvif-cameras`: Discovery must not run from HTTP-server worker tasks (was AsyncWebServer tasks).
- `host-network`: Keep ≥48 TCP sessions under IDF lwIP config (drop “Arduino default” as the only framing).
- `outbound-email`: Same async SMTP/PSRAM-attachment rules under IDF TLS/SMTP (no Arduino ESP Mail dependency on host).
- `joined-firmware-ota`: Host apply/reboot rules remain; host and slave images are IDF-built `host.bin` / `slave.bin` with identical joined ZIP contract.
- `board-role`: Clarify host and slave images are ESP-IDF builds; role-by-image and dual-host parity unchanged.

## Impact

- Host: `platformio.ini` host envs, bring-up via `app_main`, `WebConsole`/HTTP, Wi‑Fi/MQTT, SMTP, FS (LittleFS), OTA apply, InterChip host, P4 `esp_hosted`, interconnect TCP, all host stores.
- Slave: `esp32-c6-slave` → ESP-IDF; Zigbee coordinator (esp-zigbee / zboss), InterChip slave SPI, slave-side OTA apply, device/user runtime maps; SPI opcodes unchanged.
- Dependencies removed/replaced: Arduino core as app framework, ESPAsyncWebServer/AsyncTCP on host, Arduino-oriented MQTT/mail wrappers as needed; Arduino `Update` → IDF `esp_ota_*` on host and slave apply paths.
- Joined Update ZIP layout and ADR 0008 order unchanged.
- Approved fragile zones will be edited: `smtp-send`, `firmware-update-process`, `slave-spi-ota`.
