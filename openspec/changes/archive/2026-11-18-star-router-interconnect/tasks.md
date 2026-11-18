# Tasks

## 1. Settings store and device router id

- [x] 1.1 Add interconnect settings persistence (router name ≤32, role Main/Repeater, one TCP port default 10241, checkAlive 5–600 default 60, zigbee search interval, paired table cap 7 Repeaters / 8 routers total, per-peer assigned channel) and verify GET/POST round-trip after reboot

- [x] 1.2 Add `routerId` to host `DeviceTopicEntry` / devices JSON / list/save/export paths (default 0; omitted loads as 0) and verify a saved remote id survives reboot and list APIs
- [x] 1.3 Ensure remote `routerId != 0` devices are not treated as local SPI sync targets and verify slave device dump/push only covers local (`0`) Zigbee devices
- [x] 1.4 On role change after double confirm, clear paired peers/secrets, schedule host reboot, and verify table is empty after reboot

## 2. Pairing and universal AEAD exchange

- [x] 2.1 Implement versioned header + AEAD payload (≤256 bytes, AAD bound to peer MAC, version 1) and verify round-trip plus reject on bad tag/oversize/unknown version
- [x] 2.2 Implement one-port listen/connect with PSRAM queue depth 128 (reject new when full) and handshake demux for pairing vs exchange; verify queued work runs and pairing wait does not block demuxed exchange
- [x] 2.3 Implement pairing protocol (Repeater wait ≤120 s + LED6 2 Hz; Main direct IP and subnet scan + confirm); assign router id and unique Zigbee channel (11–26); store secret; verify peer appears only after full OK and 8th Repeater is refused
- [x] 2.4 Implement universal exchange handlers for enum types `just_booted`, `variable`, `check_alive`, `zigbee_packet`, `search_start`, `search_stop`, `rebind`, `rebind_result` with always-OK reply; verify Main rejects unpaired peers
- [x] 2.5 Implement Repeater `just_booted` + Main separate `variable` pushes (intervals + channel); Main param/channel recalc and Main reboot push; Main-only `check_alive`; verify liveness, intervals, and applied channel on Repeater

## 3. Zigbee forward, inject, route, search

- [x] 3.1 On Repeater, forward non-duplicate local SPI Zigbee events as `zigbee_packet` (duplicate = IEEE+cmd+identical payload in recent window) and verify Main In counters increase
- [x] 3.2 On Main, inject received `zigbee_packet` into the existing SPI ingest path and verify live/found/status match an equivalent local event with source `routerId`
- [x] 3.3 Branch outbound commands by `routerId` (0 → local SPI; else → `zigbee_packet` to Repeater; offline Repeater blocks with clear error) for all device command kinds and verify remote IEEE is not SPI-enqueued locally
- [x] 3.4 Multi-router search: local + `search_start` to Repeaters; optional `search_stop`; found via forwarded joins only; verify found UI shows source router for non-zero ids
- [x] 3.5 Route rebind by `routerId` (`rebind` / `rebind_result`) with 30 s wait locally and remotely; verify success, pairing-fallback, timeout failure, and registry kept

## 4. Web console and export

- [x] 4.1 Add System → Interconnection tab after Log (admin): name, role (double-confirm clear peers), port+Save, checkAlive (Main), Main table+Pair, Repeater Pair to host modal + Main status; verify role UI and confirms
- [x] 4.2 When role is Repeater, hide blocked sections per spec; keep Hardware allow-list and Maintenance Reboot; verify automations/similar engines do not run on Repeater
- [x] 4.3 Include interconnect slice in global settings export/restore and Maintenance hint/confirm copy; verify export/restore of paired table
- [x] 4.4 Add Maintenance Interconnection card Export/Import (match existing by MAC as authoritative; channel reassign on conflict); verify file round-trip and that the card is hidden on Repeater
- [x] 4.5 Rename Zigbee search window label; on Repeater show channel and search window read-only; verify Main editable and Repeater read-only
- [x] 4.6 Show Main↔Repeater online indicators and Devices offline/gray + blocked commands when that Repeater is offline; verify both Interconnection and Devices reflect peer liveness

## 5. Integration check

- [ ] 5.1 End-to-end: pair Main↔Repeater, Repeater reboot hello+variables, search+save remote device, report forward, remote command, remote rebind, checkAlive online; verify In/Out and indicators
