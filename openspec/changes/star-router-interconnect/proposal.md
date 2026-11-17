# Proposal

## Why

A single Zigbee coordinator covers one radio neighborhood. Operators need one Main gateway that aggregates several remote radios (Repeaters) over LAN so devices, search, and commands appear in one console without MQTT mesh complexity. Short TCP sessions between hosts must stay light on RAM and handshake time, so security uses a pairing-time shared secret and AEAD—not TLS-per-exchange.

## What Changes

- Add System → **Interconnection** tab (after **Log**) with router name (max 32), network role (**Main** | **Repeater**), pairing TCP port (default **10241**, Save), and checkAlive interval (5–600 s, default 60; Main-owned).
- **Main**: paired-routers table (Name, MAC, IP, Port, online indicator, devices, In, Out) and **Pair** dialog (IP optional, port default 10241; subnet scan when IP empty; confirm found peer; full pairing).
- **Repeater**: **Pair to host** (open pairing port up to 120 s, modal countdown + Cancel, host RED LED 2 Hz); show Main IP/name/MAC/online; hide operator sections that must not run on a slave router (Place, Devices, Automation, Cameras, Scenes, Consoles, Security, most Maintenance/Hardware—with listed Hardware exceptions and Reboot kept).
- Pairing / system-parameter / data-exchange protocols over **short TCP sessions** protected by **shared-secret AEAD** established at pairing (not SSL/TLS).
- Persist **router id** (4-byte) on each paired router and on each device record (`0` = local); Main routes Zigbee commands by `router_id`; Repeaters forward non-duplicate Zigbee packets to Main; Main injects them into the existing host Zigbee ingest path.
- Multi-router device search: Main and all Repeaters open search; found rows show source router; save stores `router_id`.
- Main pushes shared params (**checkAlive interval**, **zigbee search interval**) to all Repeaters on change and again after Main reboot.
- Include interconnect settings and paired-router table in global settings export/restore (not Wi‑Fi).
- **Out of scope this change:** MQTT device forwarding from Repeaters; automations/consoles/history “working” on Repeater role.

## Capabilities

### New Capabilities

- `router-interconnect`: STAR Main/Repeater roles, Interconnection UI, pairing and short-session AEAD protocols, online/checkAlive, shared network params, Zigbee packet forward/inject, and Repeater function blocking.

### Modified Capabilities

- `device-registry`: Persist `router_id` on registered (and found) Zigbee devices; multi-router search accumulation; route identity for local vs remote radio.
- `web-console`: System → Interconnection tab; role-gated sidebar/System visibility on Repeater; Maintenance export/restore hint and interconnect slice; device/found UI shows router when non-local.
- `status-rgb-led`: Host RED LED blinks at 2 Hz for the Repeater pairing wait window (cancel/timeout/success ends blink).

## Impact

- Host firmware: new interconnect settings store, TCP client/server short sessions, AEAD (mbedtls), pairing/scan state machine, packet forward/inject hooks in Zigbee command and SPI-event ingest paths, RED LED pairing mode.
- Web console (`data/index.html`, shared CSS as needed): Interconnection tab, pairing modals, paired table, Repeater chrome hiding.
- APIs: interconnect get/save, pair start/cancel/status, paired list; device JSON gains `routerId` (or equivalent); settings export/restore slice.
- Device store / `DeviceTopicEntry`: new persisted `router_id`.
- Slave Zigbee radio: **no required source change** for v1 (host-authoritative `router_id`; Repeaters use their local SPI as today).
- Firmware: bump `FIRMWARE_VERSION_HOST` build only for this propose (no slave bump).
