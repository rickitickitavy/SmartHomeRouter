# Tasks

## 1. Persist publishToMqtt

- [x] 1.1 Add `publishToMqtt` on `DeviceTopicEntry` (and clone/clear/upsert paths); verify compile and that mqtt-transport records do not require the field
- [x] 1.2 Load/save `devices.json` and list APIs: missing key on non-MQTT → true; new non-MQTT create defaults false; verify round-trip after reboot and export/import
- [x] 1.3 Bump `FIRMWARE_VERSION` build by 1 on apply (propose already set `3.4.21`); verify `include/Defines.h`

## 2. Gate MQTT publish and subscriptions

- [x] 2.1 Skip `publishDeviceState` (and any Zigbee→MQTT state/availability publish using device topics) when non-MQTT and `publishToMqtt` is false; verify no `MQTT send` for that device’s state topic
- [x] 2.2 Skip command-topic (and related) subscriptions in `subscribeDeviceCommands` when flag is false; refresh subscriptions after device save; verify subscribe list omits those topics
- [x] 2.3 Leave `mqtt` transport subscribe/publish behavior unchanged; verify an mqtt device still gets state/availability subscriptions

## 3. Edit device dialog

- [x] 3.1 Group state/command/availability topic fields in an **MQTT** container; for non-MQTT show **Publish to MQTT** before topics; hide topics when unchecked; verify mqtt transport has no checkbox and topics stay visible
- [x] 3.2 Wire save/load of the checkbox to `publishToMqtt`; verify Save then reload shows the same checked state and topic visibility

## 4. Integration check

- [x] 4.1 Manual: legacy zigbee still publishes until flag cleared; new zigbee starts with flag off (no state MQTT / no command subscribe); enable flag, set topics, confirm publish and subscribe resume

## Workflow follow-up

- Archive the change after review when implementation is complete.
