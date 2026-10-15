# Design

## Context

See proposal.md — Why. Devices live in `DeviceTopicEntry` / `devices.json`. State publish is `MqttClient::publishDeviceState`; subscriptions are built in `subscribeDeviceCommands` (Zigbee command topics; MQTT devices get state/availability). The edit dialog in `data/index.html` already has state/command/availability fields.

## Goals / Non-Goals

**Goals:**

- Persist `publishToMqtt` for non-MQTT devices with legacy-true / new-false defaults.
- Skip MQTT state publish and device MQTT subscriptions when the flag is false.
- Console: MQTT group, checkbox for non-MQTT, hide topics when off.

**Non-Goals:**

- Changing MQTT-transport device model or SoftAP limits.
- Syncing this flag to the slave over SPI (host-only MQTT concern).
- Removing stored topic strings when the checkbox is unchecked (keep last values for when re-enabled).
- Changing automation Listen configuration (only host MQTT client subscriptions).

## Decisions

1. **JSON key `publishToMqtt`.** Bool on non-MQTT records. Omit or ignore on `mqtt` transport. Load: missing → `true`. New create path: default `false` in upsert/save when transport is not mqtt and body omits the key.

2. **Gate at publish and subscribe.** In `publishDeviceState` (and any sibling that publishes device state/availability for Zigbee onto MQTT), return early if `!publishToMqtt && transport != mqtt`. In `subscribeDeviceCommands`, skip Zigbee command-topic `keepOrSubscribe` when flag is false. Re-run subscribe after device save so toggles take effect.

3. **UI.** Wrap topic inputs in an MQTT group/card. Non-MQTT: checkbox first; `hidden` or equivalent on topic rows when unchecked. Save sends `publishToMqtt` from the checkbox. MQTT devices: no checkbox; topics always shown in the group.

4. **Entity life cycle.** Round-trip through single-item save, list JSON, export/import, reboot, edit dialog — including when readonly surfaces omit the checkbox for mqtt transport only.

## Risks / Trade-offs

- [Topics remain in JSON while UI hidden] → Accept; avoids wiping operator-configured topics on accidental uncheck.
- [Inbound MQTT commands stop when flag off] → Intended; Manual command / Zigbee path still works.
- [Large devices.json migration] → Missing key → true preserves old fleets.

## Migration Plan

Flash host. Existing devices keep MQTT behavior (missing → true). New Zigbee registrations start with publish off until the operator enables it. Rollback: previous firmware ignores the extra JSON key.

## Open Questions

None.
