## 1. Slave recovery B — deferred apply failure

- [x] 1.1 On deferred radio apply hard failure (coordinator not started when required, Zigbee lock miss, cannot arm Search blue / open join, etc.), `markZigbeeLost` and reboot the slave **immediately**; clear Zigbee-started for Ping
- [x] 1.2 Do not trip on zero devices found or missing end-device replies alone; do not trip during firmware-transfer quiet
- [x] 1.3 Do not add a new Ping/alive/heartbeat checker; do **not** add a per-minute Zigbee self-test probe

## 2. Slave honest permit-join / radio CmdResult

- [x] 2.1 Stop early ok on `SpiCmdPermitJoin` at queue time; success only after open/close apply (blue pairing armed on open)
- [x] 2.2 Failure result when apply cannot run (feeds section 1)
- [x] 2.3 Apply open/close under timed Zigbee lock; fail closed on lock miss

## 3. Host — born interval Zigbee collect without MQTT topic

- [x] 3.1 Run Zigbee status collect every `bornIntervalMin` in normal operation even when server-born topic is empty
- [x] 3.2 Keep MQTT topic subscribe/publish born as additive when topic is set
- [x] 3.3 Keep “devices did not answer” from meaning slave failure

## 4. Host — Search applied wait + existing Ping reset

- [x] 4.1 Search/permit-join wait for applied success/failure (3–5 s); no enqueue-only “started”
- [x] 4.2 On failure/timeout while Zigbee was up: log, reset via existing lost-Zigbee path
- [x] 4.3 Confirm Ping Zigbee-started clear still resets slave (complements B)

## 5. Console — interval on Hardware

- [x] 5.1 Move BORN_INTERVAL_MIN editor to System → Hardware; keep SERVER_BORN_TOPIC on MQTT
- [x] 5.2 Wire hardware GET/POST (or equivalent) for `bornIntervalMin`; keep export/restore
- [x] 5.3 Sync operator UI only (admin Hardware/MQTT); no simple-user surface for these fields

## 6. Versions and smoke

- [x] 6.1 Confirm propose bumps host **4.5.18** / slave **4.2.17**; bump again on apply if sources change
- [x] 6.2 Build host + `esp32-c6-slave`; do not flash unless the human approves
- [x] 6.3 Smoke: empty MQTT topic still interval-collects; deferred apply death → slave reboot + host Ping reset; Search no longer false-starts without blue/apply
