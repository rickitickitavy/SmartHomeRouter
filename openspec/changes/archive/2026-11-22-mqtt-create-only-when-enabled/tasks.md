## 1. Reconcile lifecycle

- [x] 1.1 Add host `reconcileMqttObjectsFromSettings` (or equivalent): create/destroy `MqttClient` / `MqttBroker` per SERVER TYPE matrix; wire handlers, `setLocalBroker`, `begin` on create; safe teardown on destroy
- [x] 1.2 Call reconcile at host bring-up instead of unconditional `new`; call again from MQTT reload pending path
- [x] 1.3 Null-guard loop `dispatch` and any remaining unconditional client/broker uses

## 2. Verify

- [x] 2.1 Build host (`esp32-s3-host` and/or `esp32-p4-host`); do not flash unless the human approves
- [x] 2.2 Confirm host **4.6.07**; slave unchanged
- [x] 2.3 Smoke: boot Disable → no client/broker; save Remote → client only; Local → both; back to Disable → both destroyed; Status/LED match
