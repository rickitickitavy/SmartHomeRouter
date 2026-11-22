## Why

The host always constructs `MqttClient` (and its `PubSubClient`) and `MqttBroker` at boot even when MQTT SERVER TYPE is `disable`. Those objects are unused and waste RAM. Operator rule: do not create unused entities.

## What Changes

- **MqttClient:** create only when SERVER TYPE is `remote` or `local`. When type is `disable`, no client object.
- **MqttBroker:** create only when SERVER TYPE is `local`. When type is `disable` or `remote`, no broker object.
- **Lifecycle:** create on first need (boot if already enabled, or MQTT save that enables); **destroy** when settings change away from the type that needs that object (Disable destroys client; leaving Local destroys broker; Disable destroys both).
- Null-safe dispatch / publish / Status paths already mostly check pointers; fix any remaining unconditional calls (e.g. loop `mqttBroker->dispatch` / `mqttClient->dispatch`).
- Console MQTT save still applies without router restart; ensure / recreate / destroy happens on that reload path.
- S3 and P4 host; slave unchanged. Scope is MQTT only (not a host-wide unused-entity sweep).

## Capabilities

### New Capabilities

- `mqtt-client`: Host MQTT client object exists only when SERVER TYPE is remote or local; destroy when Disable.

### Modified Capabilities

- `mqtt-broker`: Broker object exists only for Local (not merely “not listening” while allocated).

## Impact

- Host boot and MQTT settings reload in `main.cpp` (+ wire-up of begin/handlers).
- Lower idle RAM when MQTT is disabled (factory default).
- Host propose bump **4.6.06**; slave unchanged.
