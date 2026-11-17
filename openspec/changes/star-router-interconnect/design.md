# Design

## Context

See `proposal.md` for motivation. Today each gateway is a single host+slave Zigbee radio with device registry and ingest entirely on the host (`DeviceTopicMap`, `onHostSpiEvent` / `ZigbeeSpiProxy`, command path through `applyRegisteredDeviceCommand*`). There is no peer-host TCP control plane. System tabs are Log → Update → Hardware → Maintenance; Hardware and settings export already cover SPI/LED/queue/low-memory knobs. Crypto in-tree is mbedtls digests/HMAC and stack TLS for SMTP—not application AEAD. Firmware bump for this propose: host build only (no slave source required for v1).

## Goals / Non-Goals

**Goals:**
- Host-side STAR interconnect: settings, pairing, short-session AEAD transport, checkAlive, param push, Zigbee forward/inject/route by `router_id`.
- Operator UI on System → Interconnection plus Repeater chrome allow-list.
- Persist `router_id` on devices; multi-router search aggregation on Main.
- Include interconnect in global export/restore.

**Non-Goals:**
- MQTT device forwarding from Repeaters (later).
- Making automations/consoles/history “work” on Repeater role.
- TLS/SSL for interconnect sessions.
- Persistent long-lived TCP between routers.
- Slave SPI schema / `router_id` on slave (host-authoritative).
- Changing fragile OTA/SPI-OTA or SMTP worker paths.

## Decisions

### 1. Shared-secret AEAD over short TCP (not TLS)
- **Choice:** Pairing establishes a per-peer secret; every later session is TCP + AEAD (prefer ChaCha20-Poly1305 or AES-GCM via mbedtls) with explicit nonce/counter and MAC binding to paired identity.
- **Why:** Protocols are connect→exchange→OK→disconnect; TLS handshake cost/RAM per short session is unacceptable on ESP32.
- **Alternatives:** mTLS (rejected for RAM/time); shared TLS PSK (still heavy); plaintext token (too weak).

### 2. Host-only `router_id` and routing branch
- **Choice:** Add `uint32_t routerId` to host `DeviceTopicEntry` / JSON (`routerId`). Local = `0`. Outbound: branch before SPI enqueue—remote ids use interconnect data-exchange to the paired Repeater. Inbound from Repeater: decode packet and call the same host ingest used for `SpiEvtAttrReport` / join events (shared helper), not a parallel MQTT-only path.
- **Why:** Host already owns registry and ignores slave dumps as authoritative; avoids SPI sync layout change and slave bump.
- **Alternatives:** Sync `router_id` to slave (extra complexity, little benefit for v1).

### 3. Interconnect module + listen ports
- **Choice:** New host module (e.g. `RouterInterconnect`) owning settings store, paired table, pairing state machines, exchange listen port (Main’s port advertised in pairing), AEAD session helper, counters (In/Out), and checkAlive timer. Use blocking or short-task TCP client/server suitable for one session at a time to bound peak RAM.
- **Why:** Keeps WebConsole thin; matches short-session model.
- **Alternatives:** Fold into WebConsole (harder to test); long-lived sockets (out of product protocol).

### 4. Handshake and secret establishment
- **Choice:** Cleartext handshake bytes during pairing window only (`ImHere` / `copy`), then exchange identity fields and derive/store shared secret (e.g. random 32-byte key from Main, or ECDH if cheap—default **Main generates random key**, sends during pairing after handshake, both persist). Subsequent sessions AEAD with that key; Main rejects unknown MAC on inbound.
- **Why:** Matches user’s pairing steps; keeps first implementation small.
- **Alternatives:** ECDH (stronger, more code); pre-shared factory key (weaker fleet-wide).

### 5. Zigbee search interval mapping
- **Choice:** Main-owned “zigbee search interval” is the permit-join / search window duration pushed to Repeaters; align storage with existing search duration semantics (`permitJoinOnBootSec` / search window) so one duration drives Main-local and remote search when Main starts Search.
- **Why:** No separate orphan interval; user asked for a shared Main-controlled search duration.
- **Alternatives:** Independent new knob disconnected from local search (confusing).

### 6. Repeater UI gating
- **Choice:** Gate sidebar and System panels from persisted network role on the console (and enforce on sensitive APIs where practical). Hardware allow-list and Maintenance=Reboot-only as specified.
- **Why:** Spec requires hidden functions; role is the single switch.

### 7. RED LED 2 Hz
- **Choice:** Extend `StatusRgb` with an interconnect-pairing blink mode on host LED6 at 2 Hz; priority critical > firmware-update 5 Hz > interconnect 2 Hz.
- **Why:** Matches pairing UX; reuses existing LED6 red channel.

### 8. Export/restore slice
- **Choice:** Add `interconnect` object to settings export/restore (role, name, ports, intervals, paired peers including secret material needed to restore peering). Update Maintenance hint copy.
- **Why:** Project rule: persisted features join global export unless excluded.

## Risks / Trade-offs

- **[Risk] AEAD short sessions under Zigbee burst load → latency/heap pressure** → Mitigation: one in-flight interconnect session; drop/coalesce duplicates on Repeater; bounded payload size; reuse buffers.
- **[Risk] Subnet scan noisy / slow** → Mitigation: skip paired IPs; handshake-only probe; operator confirm; cancelable; reasonable per-IP timeout.
- **[Risk] Pairing secret sniffed on LAN during wait** → Mitigation: only while explicit 120 s pairing window + LED/modal; document physical/LAN trust; future can upgrade to ECDH without changing STAR UX.
- **[Risk] Injecting remote packets into local ingest double-counts or loops** → Mitigation: tag ingest source as interconnect; never re-forward Main→Repeater traffic back to Main; duplicate filter on Repeater.
- **[Risk] Remote devices still SPI-synced as local** → Mitigation: do not push remote-`router_id` devices to slave `SpiCmdSetDevice` (or push only local `router_id==0`).
- **[Risk] Repeater still has stored automations that must not run** → Mitigation: skip AutomationEngine (and similar) scheduling when role is Repeater, not only hide UI.

## Migration Plan

- Existing single-gateway installs default role `Main`, empty paired table, `routerId=0` on all devices—behavior unchanged.
- No slave OTA required for this feature.
- Restore of interconnect slice replaces peering config; operator may need to re-pair if secrets omitted in older files.

## Open Questions

- Exact AEAD framing bytes (length prefix, nonce size, AAD contents) can be fixed at implement time without changing requirements.
- Maximum paired Repeater count (suggest small fixed cap, e.g. 8) chosen at implement time from RAM.
