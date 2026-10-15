# Proposal

## Why

A registered Zigbee `onOff` device reports both ON and OFF on the slave, but after rapid toggles the host MQTT state topic often receives only `ON`. Pure MQTT devices are fine. Logs show `Kept channel status ep=…; ignored a later reply` on the OFF report while `Device event On/Off … val=0x0` still appears — the slave logs the attribute change but does not forward it to the host, so routers never see OFF.

The intended hold rule is: within ~8 seconds, do **not** re-publish the **same** status as the last published value for that channel. Opposite values must always publish. The current code does the inverse (allows duplicates, suppresses changes).

## What Changes

- Fix `StatusAnswerHold` / `suppressStatusReplacement` polarity: suppress only when the new On/Off value equals the last held (last published) value and the hold window has not expired; always forward a different value and update the hold.
- Ensure each accepted On/Off change still reaches the host over SPI and is published on the device state MQTT topic as `ON` or `OFF`.
- No **BREAKING** SPI or MQTT topic changes.
- `FIRMWARE_VERSION` build bumped to `3.4.16` at propose time.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `zigbee-slave-radio`: On/Off hold MUST suppress only a duplicate of the last published value within the hold window; a later opposite value for the same IEEE and endpoint MUST be forwarded to the host.
- `mqtt-device-topics`: Clarify that Zigbee-sourced On/Off state publishes include OFF after rapid toggles the same way ON is published (host already maps whatever the slave forwards).

## Impact

- Slave: `ZigbeeCoordinator::suppressStatusReplacement` / `StatusAnswerHold` used from `handleAttributeReport` and `handleLightStateWithSource` (log line `Kept channel status…`).
- Host: unchanged publish path (`ZigbeeSpiProxy` → `onLightState` → `MqttClient::publishDeviceState`) once OFF reports are forwarded again.
- Unchanged: MQTT topic mapping, boot status-read queueing, one-in-flight outbound commands, web console.
- Assumption from field log: slave sees `val=0x0` and `val=0x1` for `House-hall-main-light`; only `MQTT send … data=ON` appears on the host.
