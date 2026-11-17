# Tasks

## 1. Settings store and device router id

- [ ] 1.1 Add interconnect settings persistence (router name ≤32, role Main/Repeater, pairing port default 10241, checkAlive 5–600 default 60, zigbee search interval shared with search window semantics, empty paired table) and verify GET/POST round-trip after reboot
- [ ] 1.2 Add `routerId` to host `DeviceTopicEntry` / devices JSON / list/save/export paths (default 0; omitted loads as 0) and verify a saved remote id survives reboot and list APIs
- [ ] 1.3 Ensure remote `routerId != 0` devices are not treated as local SPI sync targets and verify slave device dump/push only covers local (`0`) Zigbee devices

## 2. AEAD transport and pairing

- [ ] 2.1 Implement short-session TCP + shared-secret AEAD helper (one session at a time) and verify encrypt/decrypt round-trip and reject on bad tag
- [ ] 2.2 Implement Repeater pairing wait (listen pairing port ≤120 s, Cancel, StatusRgb LED6 2 Hz) and verify timeout/cancel stops listen and blink
- [ ] 2.3 Implement Main pairing (direct IP and subnet scan + confirm) per pairing protocol, assign unique non-zero router id, store peer + secret, and verify successful pair appears in paired table only after full OK
- [ ] 2.4 Implement system-parameter exchange and checkAlive; on Main interval change and Main reboot push checkAlive + zigbee search interval to all Repeaters; verify Repeater stores values and Main online/gray indicators track liveness

## 3. Zigbee forward, inject, route, search

- [ ] 3.1 On Repeater, forward non-duplicate local Zigbee packets to Main via data-exchange type packet and verify Main counters In increase
- [ ] 3.2 On Main, inject received Repeater packets into the existing host Zigbee ingest path and verify live/status updates match an equivalent local event
- [ ] 3.3 Branch outbound Zigbee commands by `routerId` (0 → local SPI; else → paired Repeater data-exchange) for all device command kinds and verify remote IEEE is not SPI-enqueued locally
- [ ] 3.4 Multi-router search: Main search starts local + commands Repeaters; found list carries source `routerId`; register copies it; verify found UI shows source router for non-zero ids

## 4. Web console and export

- [ ] 4.1 Add System → Interconnection tab after Log (admin): name, role, pairing port+Save, checkAlive (Main), Main table+Pair dialog, Repeater Pair to host modal + Main status; verify role switches UI correctly
- [ ] 4.2 When role is Repeater, hide Place/Devices/Automation/Cameras/Scenes/Consoles/Security; Maintenance except Reboot; Hardware except SPI SPEED, RF update SPI speed, blue/green LED brightness, use queue for base device commands, low memory threshold; verify allow-list and that Repeater skips running automations/similar engines
- [ ] 4.3 Include interconnect slice in settings export/restore and Maintenance hint/confirm copy; verify export contains interconnect and restore replaces paired table when present

## 5. Integration check

- [ ] 5.1 End-to-end: pair Main↔Repeater, remote device search+save with `routerId`, report forward to Main, command from Main to remote device; verify online indicators and In/Out counters move as expected
