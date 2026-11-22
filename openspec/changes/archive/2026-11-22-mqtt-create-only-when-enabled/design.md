## Context

Today (`main.cpp` host bring-up):

```cpp
mqttClient = new MqttClient(...);  // always
mqttBroker = new MqttBroker(...);  // always
mqttClient->setLocalBroker(mqttBroker);
mqttClient->begin(...);
// loop always: mqttBroker->dispatch(...); mqttClient->dispatch(...);
```

`MqttClient` ctor allocates `PubSubClient`. Disable only skips connect inside `dispatch`. Broker already “starts only for local” for listening, but the object still exists.

Operator choices: **MQTT scope only**; **create on enable / destroy on disable** (not park).

## Goals / Non-Goals

**Goals:**

- No `MqttClient` when SERVER TYPE is `disable`.
- No `MqttBroker` unless SERVER TYPE is `local`.
- Boot constructs only what current settings need.
- MQTT save without reboot reconciles objects: create missing needed ones, destroy ones no longer needed.
- All call sites tolerate null (Status, publish, subscribe, loop).

**Non-Goals:**

- Changing MQTT topic schema, broker protocol, or console SERVER TYPE UI.
- Sweeping other unused host entities (history, email, etc.).
- Slave changes.

## Decisions

1. **Needed matrix**

| SERVER TYPE | MqttClient | MqttBroker |
|-------------|------------|------------|
| disable     | absent     | absent     |
| remote      | present    | absent     |
| local       | present    | present    |

2. **Reconcile helper**  
   Single host function (e.g. `reconcileMqttObjectsFromSettings`) called from boot after settings load and from `hostMqttReloadPending` handling:  
   - If need client and null → `new`, set handlers/`setLocalBroker`, `begin`.  
   - If need broker and null → `new`, set message handler, link to client.  
   - If broker not needed and non-null → stop listen if any, unlink from client, `delete`, null.  
   - If client not needed and non-null → disconnect/teardown, `delete`, null.  
   Order: tear down unneeded broker before client when leaving local; create broker before attaching local client when entering local.

3. **Loop**  
   Call `dispatch` only when the pointer is non-null (Repeater already skips MQTT block).

4. **LED / Status**  
   Unchanged semantics: Disable → not connected / broker not listening; absence of objects implies the same Status/LED outcomes as today when disabled.

5. **Destroy safety**  
   Clear connect-task / subscription state in existing destructors; do not leave `hostMqttReload` calling into freed objects.

6. **Version**  
   Host **4.6.06**; slave unchanged.

## Risks / Trade-offs

- **[Risk] Use-after-free on reload** if a deferred MQTT callback runs after destroy → Mitigation: destroy only on the deferred reload pump; null-check message paths; stop broker listen before delete.
- **[Risk] Missed `begin` after create-on-enable** → Mitigation: centralize create+wire+begin in reconcile.

## Migration Plan

- Flash host **4.6.06**. No settings format change. Factory Disable uses less RAM immediately.

## Open Questions

None — operator selected scope A and lifecycle A.
