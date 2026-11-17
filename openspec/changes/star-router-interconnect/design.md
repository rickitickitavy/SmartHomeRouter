# Design

## Context

See `proposal.md` for motivation. Host owns device registry and Zigbee ingest (`onHostSpiEvent` / `FoundDeviceList` / command path). No peer-host control plane yet. Crypto in-tree is mbedtls digests/HMAC and stack TLS—not application AEAD. Host-only for v1 (no slave bump).

## Goals / Non-Goals

**Goals:**
- Pairing protocol unchanged; everything else uses one universal typed AEAD exchange.
- One TCP port for pairing + exchange; PSRAM queue depth 64 (drop newest when full).
- SPI-equivalent `zigbee_packet` so Main ingest treats socket frames like local SPI events.
- Repeater boot hello → Main shared `variable` push; checkAlive Main→Repeater; role change clears peers with double confirm.

**Non-Goals:**
- TLS; typed replies; `request` / `search_found`; known-IEEE list push to Repeaters; MQTT forward; slave `router_id`.

## Decisions

### 1. Two protocols only
- **Pairing:** existing cleartext sequence (`ImHere`/`copy`, identity, port, secret, router id); persist peer only after both OKs.
- **Universal exchange:** listen/connect on the **same** configured port → `ImHere`/`copy` → one AEAD message `type_u8 || payload` → reply **`OK`** → disconnect. Either side may initiate.
- After the shared handshake, the listener **demultiplexes** by what follows (pairing cleartext steps vs AEAD blob). No global “pairing-only” lock; normal exchange traffic may continue during Pair-to-host.

### 2. v1 type enum
| Value | Name | Direction (typical) | Payload |
|------:|------|---------------------|---------|
| 0 | reserved | — | — |
| 1 | `just_booted` | Repeater → Main | host firmware version string |
| 2 | `variable` | Main → Repeater | name + value (checkAlive interval, zigbee search window, Zigbee channel) |
| 3 | `check_alive` | Main → Repeater | empty or small nonce |
| 4 | `zigbee_packet` | either | SPI event frame bytes (same layout host already parses) |
| 5 | `search_start` | Main → Repeater | duration / search window |
| 6 | `search_stop` | Main → Repeater | empty |
| 7 | `rebind` | Main → Repeater | IEEE (8 bytes) |
| 8 | `rebind_result` | Repeater → Main | IEEE + outcome (success / pairing fallback / failure) |

One `zigbee_packet` type for reports and commands; direction + payload decide meaning.  
Rebind: Main sends `rebind` → `OK`; Repeater runs local SPI rebind; Repeater sends `rebind_result`; Main API waits **30 s** (same for local SPI rebind).

### 3. AEAD framing
- Single AEAD blob: `type (1) || payload`.
- **Max plaintext payload 256 bytes** (matches `SPI_MAX_PAYLOAD`; enough for SPI-equivalent frames).
- AAD bound to peer **MAC**; shared secret from pairing (Main generates random key).
- Nonce/counter layout at implement time; reject bad tag; Main accepts exchange only for paired MAC/secret.

### 4. One port + request queue
- Default port **10241** for pairing and all exchanges.
- Outbound/accept **queue depth 64**, allocated in **PSRAM**. When full, **drop the newest** job.
- Pairing wait does not freeze other interconnect work; demux per accepted session.

### 5. Max routers in STAR = 8
- Cap **8 routers total**: 1 Main + **≤7** paired Repeaters.

### 5b. Unique Zigbee channels (Main-assigned)
- Every router uses a different channel from **11–26**.
- Assign at pairing; re-send on `just_booted` and on Main recalculation via `variable`.
- Repeater applies channel locally; UI shows it **read-only** on Repeater.

### 6. `zigbee_packet` = SPI-equivalent emulation
- Same event shape as SPI ingest; Repeater forwards all non-duplicate RX; Main injects like local SPI with `routerId`.

### 7. Boot and shared params
- `just_booted` then separate `variable` sessions (checkAlive interval, zigbee search window, channel).
- Same pushes on Main change/recalc/reboot.

### 8. Search
- Local + `search_start`; finds via forwarded joins; no known-list push; optional `search_stop`.
- UI label for search duration: **“Zigbee search window (sec)”** (replaces `PERMIT_JOIN_ON_BOOT_SEC` caption). On Repeater that field is **visible read-only** (Main-owned).

### 9. Role change
- Two confirms, then clear paired peers/secrets.

### 10. Export/restore / Import
- Global settings include interconnect slice.
- Maintenance **Interconnection** Export/Import (confirm → replace).
- On Import: peers that **match already existing** peers are **authoritative** (keep existing assignment/identity); newly imported peers are added; if channels conflict, **re-run** channel assignment with existing peers authoritative.

### 11. Rebind via Repeater
- `router_id == 0`: local SPI rebind, wait **30 s**.
- `router_id != 0`: `rebind` / `rebind_result`, wait **30 s**.

## Risks / Trade-offs

- **[Risk] Queue full drops newest** → Mitigation: counters/log; operator sees lost forwards under extreme burst.
- **[Risk] Pairing secret sniffed on LAN** → Mitigation: only during explicit 120 s window + LED/modal.
- **[Risk] Inject loops** → Mitigation: never re-forward Main→Repeater traffic back; tag ingest source.
- **[Risk] Channel pool exhausted** → Mitigation: recalculate and push; refuse pair if no free channel in 11–26.

## Migration Plan

- Default role Main, empty peers, `routerId=0` — unchanged single-gateway behavior.
- No slave OTA required.
- Role switch wipes peering after double confirm.

## Open Questions

- Exact AEAD nonce/counter byte layout (implement-time; payload max and AAD-to-MAC are fixed).
