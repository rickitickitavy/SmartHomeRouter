# Design

## Context

See `proposal.md` — Why. Envs today in-tree: PlatformIO `framework = espidf` for `esp32-s3-host` and `esp32-p4-host` (bring-up + product ports in progress); `esp32-c6-slave` still `framework = arduino`. This revision **includes the C6 slave** in the same ESP-IDF cutover so operators are not left on a permanent mixed Arduino-slave / IDF-host stack. Fragile zones approved: `smtp-send`, `firmware-update-process`, `slave-spi-ota`.

## Goals / Non-Goals

**Goals:**

- Pure ESP-IDF on **S3 host, P4 host, and C6 slave** with PlatformIO `framework = espidf`.
- Preserve every external protocol and product logic (see `specs/host-esp-idf`).
- One coherent cutover: joined ZIP ships IDF `host.bin` + IDF `slave.bin`; SPI remains compatible with the previous Arduino slave for emergency/rollback paths.
- Preserve scheduling intent: SPI async, HTTP reachable after STA/Hosted recovery, SMTP off automation path and off HTTP worker cores as today, ≥48 TCP sessions, PSRAM large JSON, low-memory fail-safe, Zigbee start-on-task and radio-pause rules.

**Non-Goals:**

- Changing HTTP route/JSON contracts, MQTT topics, SPI opcodes, joined ZIP member names, Zigbee device semantics, or automation semantics.
- Optimizing for maximum RAM beyond what falls out of dropping Arduino/Async (RAM is a side benefit, not a product change).
- Replacing LittleFS store paths or export policy (Wi‑Fi still excluded).

## Decisions

1. **Build system: PlatformIO `framework = espidf` for host and slave envs**  
   Keep one repo and join scripts. Shared root `CMakeLists.txt` selects host vs slave sources by `CONFIG_IDF_TARGET_*`.  
   *Alternatives:* pure `idf.py` split repos (rejected — splits tooling); Arduino-as-component hybrid (rejected — not “full IDF”); host-only IDF with Arduino slave forever (rejected — user directed dual migration without a broken halfway product).

2. **Boot: `app_main` + FreeRTOS tasks replace Arduino `setup`/`loop` entry**  
   Host: dedicated host service task (same “must not block on SPI/SMTP/MQTT connect” rules). Slave: `app_main` drives slave bring-up + service loop (Zigbee start task, SPI pump, OTA pump) with the same observable order as today’s `setupSlave` / slave `loop`.

3. **HTTP (host): IDF `esp_http_server`** — unchanged from prior design.

4. **FS: LittleFS via IDF VFS** — unchanged; slave keeps runtime maps without host-style console FS requirements.

5. **Wi‑Fi** — S3 native / P4 Hosted; unchanged.

6. **MQTT (host)** — IDF-friendly stack; unchanged intent.

7. **SMTP (`smtp-send`)** — unchanged intent.

8. **Joined OTA / SPI OTA (approved fragile zones)**  
   Keep ADR 0008 order and ZIP layout. Host and slave apply use IDF `esp_ota_*` / partition API. Slave SPI OTA framing, ACK/retry/timeouts, exclusive transfer, empty-last-frame rules stay; only framework flash APIs change under the hood.

9. **Zigbee (slave)**  
   Use Espressif IDF Zigbee components (`esp-zigbee-lib` / `esp-zboss-lib`) with ZCZR partition/sdkconfig. Keep `ZigbeeCoordinator` behavior and SPI event enqueue contracts. Thin Arduino API shims (`String`, `millis`, GPIO) allowed temporarily; do not reintroduce Arduino as the app framework.

10. **Thin compatibility shims**  
    Shared `include/idf_compat` for host and slave where it reduces blast radius.

11. **Versioning**  
    Host track **5.0.x**; slave bumps when slave sources change (**4.2.20+** for this dual cutover).

12. **Cutover without breaks**  
    SPI frame layout frozen across the change. Field join: program new IDF slave then IDF host via existing joined Update. USB rollback still possible per role. Do not ship a release that requires an Arduino slave after this change lands.

## Boundary-case matrix (must pass)

| Area | Preserve |
|------|----------|
| SPI async + first-step slave reset | No block of host service loop; reset before Wi‑Fi/web/MQTT/SPI task |
| Slave OTA begin/chunk/end | Exclusive in-flight frame; empty buffer retry rules |
| Joined ZIP | `slave.bin`/`host.bin` (+ optional hosted-c6); slave then host; fail leaves host |
| STA / P4 Hosted rebind | Port 80 answers; rebind from service loop only |
| HTTP PSRAM JSON | >4 KiB body/response; 507 on PSRAM fail |
| lwIP | ≥48 active TCP + sockets/listen |
| Low-memory fail-safe | Threshold → tear down HTTP server → history snapshot → reboot |
| SMTP | Non-blocking enqueue; PSRAM JPEG; log failures |
| Interconnect | Port/role/AEAD/queue/pairing unchanged |
| Zigbee start / radio pause | Same task start, pump pause, join/settings behavior |
| Export/restore | All persisted features; Wi‑Fi excluded |
| Dual-host parity | Same semantics S3 and P4 |

## Risks / Trade-offs

- **[Risk] Slave Zigbee IDF link/partition mismatch** → Mitigation: ZCZR sdkconfig + partition parity with today’s `zigbee_zczr_16MB.csv`; soak join/rebind.  
- **[Risk] SPI OTA timing after leaving Arduino `Update`** → Mitigation: keep framing; replace only flash API (`slave-spi-ota` / `firmware-update-process`).  
- **[Risk] HTTP/SMTP regressions on host** → Mitigation: boundary matrix + soak.  
- **[Trade-off] Larger single change** vs host-first then slave — accepted; user asked all migration at once without breaks.

## Migration Plan

1. Host IDF envs + slave IDF env in `platformio.ini`; CMake selects role by target.  
2. Port host Wi‑Fi/HTTP/MQTT/SPI/SMTP/OTA/interconnect (in progress / bring-up).  
3. Port slave `app_main`, InterChip slave, Zigbee coordinator, slave OTA apply.  
4. Build joined ZIPs with IDF `slave.bin` + IDF `host.bin`; field-update test.  
5. Rollback: USB flash previous images per chip if needed.

## Open Questions

- None that block this revision; component versions follow the PlatformIO ESP-IDF platform in use.
