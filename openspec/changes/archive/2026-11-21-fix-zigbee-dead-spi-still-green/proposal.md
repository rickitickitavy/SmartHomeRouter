## Why

Field failure (slave **4.2.16**, master log `z2m-log (1).txt`): SPI/green stays up; slave **ignores all Zigbee devices**; host never reboots the slave. Search with no blue blink is evidence of the same dead radio-apply path, not the whole bug.

Server-born already queues Zigbee status collect, but that is fire-and-forget: the host does not learn that deferred radio apply failed, Ping can still claim Zigbee started, and the slave never self-recovers.

Core fact: **the slave knows when RF / radio-apply is down** (same class of failure as Search not arming blue). Recovery (**B**): the slave trips Zigbee-lost and **reboots immediately**; the host still resets when Ping clears Zigbee-started. The only periodic host trigger for radio work is **server-born interval** (`BORN_INTERVAL_MIN`) — **no** extra minute Zigbee self-test (that can contend with real inbound packets). Born interval MUST run Zigbee collect **independent of MQTT topic**; if a topic is set, MQTT born still runs in addition.

## What Changes

- **Slave (B):** **THE SLAVE KNOWS RF IS DOWN** when deferred radio apply cannot run (permit-join, status reads, device commands, etc., including inability to arm Search blue). It MUST mark Zigbee lost and **reboot immediately**. Zero devices / no replies is **not** failure. No new keepalive. No periodic fake Zigbee probe.
- **Honest apply results:** SPI success only after apply (or failure). Search/permit-join: success only after open/close apply (blue on open).
- **Server-born interval only (periodic):** On `BORN_INTERVAL_MIN`, always run Zigbee status collect, even with empty MQTT topic. MQTT topic remains additive when set. Do **not** add a per-minute slave Zigbee self-test.
- **UI:** Move `BORN_INTERVAL_MIN` to System / Hardware. Keep server-born topic under MQTT.
- Propose bumps: host **4.5.18**, slave **4.2.17**.

## Capabilities

### Modified Capabilities

- `zigbee-slave-radio`: deferred apply failure → Zigbee-lost + reboot; honest permit-join/apply results; locked open/close.
- `host-slave-spi`: host still resets on Ping Zigbee-started clear; no second keepalive.
- `mqtt-device-topics` / `web-console`: born interval drives Zigbee collect independently of topic; interval control under System/Hardware; MQTT topic optional additive path.

## Impact

- Slave: deferred radio apply failure → `markZigbeeLost` / reboot; permit-join result timing.
- Host: periodic Zigbee server-born collect without requiring MQTT topic; Search/permit-join wait for applied result still useful; existing Ping lost → `enterReset`.
- Console: move born-interval field to Hardware; MQTT keeps topic.
- Fragile zones `slave-spi-ota` / `firmware-update-process`: do not change SPI OTA framing; exempt firmware-transfer quiet from false apply-lost trips the same way deferred radio is already paused.
