# Tasks

## 1. Incremental remote MQTT subscription attach

- [x] 1.1 Add attach-in-progress state so `finishRemoteSessionAttach` no longer runs a full `subscribeDeviceCommands` walk in one call — verify connect still sets online status / bridge publish, and a flag/queue shows attach pending afterward
- [x] 1.2 Process a bounded number of subscribe/unsubscribe ops per loop/`dispatch` tick until the wanted topic set matches existing born / MQTT / `publishToMqtt` rules — verify with a large device map that all required topics become subscribed within a few seconds without a continuous multi-second freeze
- [x] 1.3 Keep PubSubClient subscribe work on the Arduino loop task (same affinity as `client->loop`) — verify no second task calls `subscribe`/`publish` on the remote client during attach

## 2. Reduce attach stack/heap pressure

- [x] 2.1 Trim per-topic info logging on the attach hot path (debug and/or summary counts) so attach does not allocate dozens of `String` concatenations in one tick — verify serial no longer floods one info line per topic during connect, and a summary or debug path still exists for field diagnosis
- [x] 2.2 Optionally raise Arduino loop stack size if the platform exposes a build flag, only after pacing lands — verify build still succeeds for `esp32-s3-host` and high-watermark / soak no longer canaries

## 3. Decouple born status collect from subscribe burst

- [x] 3.1 Queue Zigbee status collect when born is handled during or immediately after subscription attach so collect runs on a later loop iteration — verify a born payload still triggers status reads, and collect is not nested inside the full subscribe walk

## 4. Soak verification

- [ ] 4.1 Reproduce the previous failure mode (remote MQTT connect with a large map, born topic active) and confirm no stack-canary reboot across several connect cycles — verify host stays up and `GET /api/status` answers during attach

## Workflow follow-up

- After apply and soak: sync specs if deltas still need merging, archive `fix-mqtt-subscribe-stack-overflow`, commit/push/PR as usual.
