# Proposal

## Why

A single Zigbee coordinator covers one radio neighborhood. Operators need one Main gateway that aggregates several remote radios (Repeaters) over LAN so devices, search, and commands appear in one console without MQTT mesh complexity. Short TCP sessions between hosts must stay light on RAM and handshake time, so security uses a pairing-time shared secret and AEAD—not TLS-per-exchange.

## What Changes

- Add System → **Interconnection** tab (after **Log**) with router name (max 32), network role (**Main** | **Repeater**), one TCP port for pairing and exchange (default **10241**, Save), and checkAlive interval (5–600 s, default 60; Main-owned).
- **Main**: paired-routers table (Name, MAC, IP, Port, online indicator, devices, In, Out) and **Pair** dialog (IP optional, port default 10241; subnet scan when IP empty; confirm found peer; full pairing).
- **Repeater**: **Pair to host** (listen up to 120 s, modal countdown + Cancel, host RED LED 2 Hz); show Main IP/name/MAC/online; hide blocked operator sections (Place, Devices, Automation, Cameras, Scenes, Consoles, Security, most Maintenance/Hardware—with listed Hardware exceptions and Reboot kept).
- **Two protocols only:** (1) pairing (unchanged sequence, establishes peer identity + shared AEAD secret + router id); (2) **universal exchange** — short TCP session, `ImHere`/`copy`, versioned header + AEAD payload (max **256** bytes, AAD bound to peer MAC), reply always **`OK`**, disconnect. Either peer may initiate to the other’s same port; a **PSRAM queue of depth 128** (reject new when full) windows work. Listener demuxes pairing vs exchange after handshake; Pair-to-host does not freeze other interconnect traffic.
- Exchange types (v1 enum): `just_booted`, `variable`, `check_alive`, `zigbee_packet`, `search_start`, `search_stop`, `rebind`, `rebind_result`.
- **Rebind** works for devices on Repeaters: Main routes by `router_id`; Repeater runs local SPI rebind; outcome returns via a follow-up `rebind_result` exchange (always-`OK` sessions cannot carry the result in-reply).
- STAR size cap: **8 routers total** (1 Main + **≤7** Repeaters).
- Zigbee **channels must be unique** across all routers in the STAR. Main assigns each Repeater’s channel automatically (pool 11–26, excluding Main and other Repeaters), at pairing and again on `just_booted` / whenever Main recalculates; push via `variable`.
- Repeater boot: send `just_booted` + version to Main; Main then pushes shared settings via separate `variable` sessions (including that Repeater’s channel). Main also pushes shared settings on change and after Main reboot. `check_alive` is Main → Repeater only.
- Persist **router id** (4-byte) on each paired router and device (`0` = local). Repeaters forward all non-duplicate Zigbee RX as `zigbee_packet` in the **same SPI event format** the host already parses (duplicate = same IEEE + SPI cmd + identical payload in a short window); Main injects them into the existing ingest path. Main filters known vs found; no known-device list push to Repeaters.
- Online indicators: Main shows each Repeater online/offline; Repeater shows Main online/offline. Devices on an offline Repeater show offline and commands are rejected/disabled with a clear reason.
- Multi-router search: Main `search_start` locally and to Repeaters; finds appear via forwarded join/`zigbee_packet`; save stores `router_id`.
- Changing network role clears the paired table after **double confirmation**.
- Include interconnect settings and paired-router table in global settings export/restore (not Wi‑Fi).
- System → Maintenance: dedicated **Interconnection** card with **Export** and **Import** (confirm then replace; match existing peers by **MAC** as authoritative; re-assign channels on conflict), in addition to the global settings export slice.
- UI: Zigbee duration label **“Zigbee search window (sec)”**; on Repeater, that field and Zigbee channel are **visible read-only**. Rebind waits **30 s** locally and remotely.
- **Out of scope:** MQTT forwarding from Repeaters; automations/consoles/history working on Repeater; typed replies; `request` / `search_found` types.

## Capabilities

### New Capabilities

- `router-interconnect`: STAR Main/Repeater roles, Interconnection UI, pairing + universal typed AEAD exchange, queue/windowing, online/checkAlive, shared params, Zigbee forward/inject, Repeater function blocking.

### Modified Capabilities

- `device-registry`: Persist `router_id` on registered (and found) Zigbee devices; multi-router search accumulation via injected join path; route identity for local vs remote radio.
- `web-console`: System → Interconnection tab; role-gated visibility on Repeater; double-confirm role change clearing peers; Maintenance Interconnection Export/Import card; global export/restore interconnect slice; device/found UI shows router when non-local.
- `status-rgb-led`: Host RED LED blinks at 2 Hz for the Repeater pairing wait window (cancel/timeout/success ends blink).
- `zigbee-device-rebind`: Rebind for registered Zigbee devices with non-zero `router_id` runs on the owning Repeater and reports the same success / pairing-fallback / failure outcomes to Main.

## Impact

- Host firmware: interconnect store, one-port TCP client/server with request queue, AEAD, pairing/scan, typed exchange handlers, SPI-equivalent inject/forward, RED LED pairing mode.
- Web console: Interconnection tab, pairing modals, role double-confirm, Repeater chrome hiding.
- APIs: interconnect get/save, pair start/cancel/status, paired list; device JSON `routerId`; settings export/restore slice.
- Device store: persisted `router_id`.
- Slave: **no required source change** for v1.
- Firmware: host build bump already done for this change propose (no slave bump).
