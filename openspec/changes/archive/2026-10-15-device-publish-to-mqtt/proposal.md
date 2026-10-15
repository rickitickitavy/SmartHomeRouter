# Proposal

## Why

Operators want Zigbee (and other non-MQTT) devices that stay on the gateway console and automations without flooding the MQTT broker. Today every registered non-MQTT device with topics still publishes state and gets command-topic subscriptions. A per-device opt-in is needed so MQTT traffic is optional.

## What Changes

- Add a persisted boolean **Publish to MQTT** (`publishToMqtt`) on registered devices whose transport is **not** `mqtt`. MQTT-transport devices are excluded (no flag; they remain MQTT-native).
- **Migration:** existing non-MQTT records missing the field load as `true` (keep current behavior). **New** non-MQTT devices (including first save from found/pairing or MQTT DEVICE path for zigbee) default to `false`.
- When `publishToMqtt` is false on a non-MQTT device: the host MUST NOT publish that device’s state (or related device MQTT traffic that uses its stored topics) to MQTT, and MUST NOT create MQTT subscriptions/listeners for that device’s command (or other device MQTT) topics.
- Web console edit-device dialog: wrap MQTT topic fields in an **MQTT** group. For non-MQTT transport, show **Publish to MQTT** checkbox above topic fields; hide topic fields when unchecked.
- `FIRMWARE_VERSION` build bumped to `3.4.21` at propose time.
- No **BREAKING** SPI frame change expected (host-only setting).

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `device-registry`: Persist `publishToMqtt` for non-MQTT devices with load defaults (missing → true; new creates → false).
- `mqtt-device-topics`: Gate state publish and device MQTT subscriptions on `publishToMqtt` for non-MQTT devices.
- `web-console`: Edit-device MQTT group, checkbox, and conditional topic fields.

## Impact

- Host: `DeviceTopicEntry` / `devices.json` / list APIs; `MqttClient::publishDeviceState` and `subscribeDeviceCommands`; device save/load paths.
- Web: `data/index.html` (and CSS if needed) device parameters dialog.
- Unchanged: MQTT-transport device behavior; Zigbee radio; Manual command over HTTP for Zigbee; automations that command devices without MQTT.
- Assumption: “listeners” means host MQTT subscriptions for that device’s topics (command / related), not automation Listen rows.
