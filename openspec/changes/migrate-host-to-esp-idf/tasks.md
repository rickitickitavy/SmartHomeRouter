# Tasks

## 1. Build scaffolding (hosts + slave)

- [x] 1.1 Switch `esp32-s3-host` and `esp32-p4-host` to PlatformIO `framework = espidf` and verify both host envs configure/compile a minimal `app_main`
- [x] 1.2 Port host sdkconfig knobs (≥48 TCP/sockets/listen, P4 hosted SDIO queues / L2 cache as today) and verify menuconfig/build reflects those floors on both hosts
- [x] 1.3 Confirm host version strings are on the `5.0.x` track (`S3-…` / `P4-…`) and boot log / firmware image embeds the prefixed host string
- [x] 1.4 Switch `esp32-c6-slave` to PlatformIO `framework = espidf` with ZCZR Zigbee components/partitions; verify slave `app_main` configures/compiles
- [x] 1.5 Confirm slave version string bumps with slave source and embeds in the slave image

## 2. Boot, FS, Wi‑Fi (host)

- [x] 2.1 Replace host `setup`/`loop` with `app_main` + host service task; verify first-step slave reset still completes before Wi‑Fi/web/MQTT/SPI task start
- [x] 2.2 Mount LittleFS (IDF VFS) with existing store paths; verify a store file round-trips on S3 and P4
- [x] 2.3 Port S3 native Wi‑Fi SoftAP/STA recovery; verify SoftAP and STA join, and console rebind after STA reconnect
- [x] 2.4 Port P4 ESP-Hosted Wi‑Fi + Hosted recovery rebind arm; verify STA/SoftAP and rebind without classic STA edges

## 3. HTTP console (host)

- [x] 3.1 Replace ESPAsyncWebServer/AsyncTCP with IDF HTTP server; register existing routes/static assets/auth and verify login + `/api/auth/me` on both hosts
- [x] 3.2 Implement listen rebind with bind-fail retry from the host service loop only; verify port 80 answers after STA/Hosted recovery and first boot bind is not torn down immediately
- [x] 3.3 Keep >4 KiB JSON body/response PSRAM paths and HTTP 507 on PSRAM failure; verify large settings restore body and large devices/status JSON
- [x] 3.4 Update low-memory fail-safe to tear down the IDF HTTP server then history snapshot then reboot; verify sequence on a forced low-heap trip (or unit/hook equivalent)

## 4. MQTT, stores, product logic (host)

- [x] 4.1 Replace Arduino MQTT libs with IDF-compatible broker/client preserving enable rules and topics; verify local broker and remote client against existing console MQTT settings
- [x] 4.2 Ensure MQTT reconnect / Wi‑Fi scan do not freeze HTTP/SPI for tens of seconds; verify Status HTTP answers during reconnect
- [x] 4.3 Smoke automations, consoles, scenes, cameras, device registry, history, and settings export/restore (Wi‑Fi excluded); verify operator UI parity on S3 and P4

## 5. Host↔slave SPI

- [x] 5.1 Port InterChip host to IDF SPI/GPIO without changing frame layout; verify `SLAVE_READY`, settings push, and a Zigbee command/event round-trip
- [x] 5.2 Verify async SPI: host service loop never blocks waiting on transfer; timeout surfaces asynchronously
- [x] 5.3 Port InterChip slave to ESP-IDF build (same frame layout); verify `SLAVE_READY` and settings/command/event round-trip with IDF host

## 6. Fragile zones — SMTP and OTA

- [x] 6.1 Rewrite `EmailSender` for IDF TLS/SMTP preserving configured gate, PSRAM queue depth 16, lazy CPU1 worker, mutex serialize, PSRAM JPEG attach; verify automation email send + account check (`smtp-send`)
- [x] 6.2 Keep HTTP workers off the SMTP core (equivalent isolation to former AsyncTCP@CPU0); verify soak under UI + email without WDT/freeze
- [x] 6.3 Retarget host apply to IDF `esp_ota_*` while keeping joined ZIP layout and ADR 0008 order; verify skip-when-equal and fail-before-host-apply leaves running app (`firmware-update-process`)
- [ ] 6.4 Preserve slave SPI OTA begin/chunk/end, exclusive in-flight, empty-last-frame retry; retarget **slave** image apply to IDF `esp_ota_*`; verify joined update with IDF `slave.bin` then IDF `host.bin` (`slave-spi-ota`)
- [x] 6.5 On P4, verify optional `hosted-c6.bin` present/needed/unreadable-version rules still hold

## 7. Slave Zigbee (IDF)

- [x] 7.1 Link esp-zigbee / zboss ZCZR on C6 IDF; port `ZigbeeCoordinator` bring-up under `app_main` with radio-pause rules unchanged
- [ ] 7.2 Verify settings → Zigbee start, permit join, rebind, attr/report events over SPI against IDF host
- [ ] 7.3 Verify Zigbee start failure / timeout reboot behavior matches pre-migration slave

## 8. Interconnect and dual-role gate

- [x] 8.1 Port STAR interconnect TCP/AEAD/pairing/exchange on IDF sockets; verify pair + exchange against a peer with unchanged protocol
- [ ] 8.2 Integration gate: IDF host (S3 and P4) + IDF slave — SPI + HTTP + MQTT + SMTP enqueue + Zigbee join smoke + joined update checklist from `design.md` boundary matrix

## Workflow follow-up

- Archive the change after review and implementation sign-off.
- Clear or refresh `.cursor/fragile-zones.allow` when the fragile-zone work is finished.
