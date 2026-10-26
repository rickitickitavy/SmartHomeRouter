# Design

## Context

See proposal.md for motivation. Status already uses a two-column `.status-stack` (`data/css/all.css`): Free memory is pinned to column 2 (`grid-column: 2; grid-row: 1 / span 2`); other groups auto-flow. Devices still shows combined `devices` / `online` plus Zigbee SPI `packetsRx` / `packetsTx` from `ZigbeeSpiProxy`. MQTT availability handling also calls `notePacketReceived()`, so the current “packets” numbers are not cleanly Zigbee-only. `GET /api/status` from `hostGatewayStatusJson` already exposes MQTT mode/status/topics/connections and Wi-Fi fields. MQTT and Zigbee device `transport` is already on `DeviceTopicEntry`; online for both uses the SPI proxy last-seen window (`isOnline` / `noteSeen` / `noteMqttOffline`).

## Goals / Non-Goals

**Goals:**
- Column placement: Devices after MQTT in column 2; Network then Connections then WiFi in column 1 (after Version / online time).
- Split device and packet telemetry by transport; new Connections telemetry.
- Additive `/api/status` keys; keep existing keys that MQTT/UI still use unless superseded in the UI.

**Non-Goals:**
- Changing MQTT SERVER TYPE, broker listen rules, or Wi-Fi join behavior.
- Persisting any of these counters (runtime telemetry only; reset on reboot).
- Redesigning Free memory / Version / MQTT mode rows beyond placement.
- Removing the MQTT group’s existing Connections row (kept as today’s combined `mqttConnectionCount`).

## Decisions

1. **Extend `/api/status`, do not add new URLs**  
   Status already polls one document. Add keys for transport-split devices, Zigbee vs MQTT packets, and connection breakdown.  
   Alternative: nested `network` / `connections` objects — acceptable if apply prefers nesting, as long as the UI can bind the same values.

2. **Device counts by transport**  
   In `hostGatewayStatusJson`, walk `topicMap` and count registered + online separately for `DeviceTransportZigbee` and `DeviceTransportMqtt` using existing `isOnline`. Expose e.g. `zigbeeDevices`, `zigbeeOnline`, `mqttDevices`, `mqttOnline`. Keep legacy `devices` / `online` as totals (sum) for any other consumer; Status UI binds only the split fields.  
   Alternative: drop legacy keys — rejected; additive is safer.

3. **Zigbee vs MQTT packet counters**  
   - Zigbee: keep `packetsRx` / `packetsTx` on `ZigbeeSpiProxy` for SPI/Zigbee path only; stop counting MQTT availability/state into those counters.  
   - MQTT: add host-side `mqttPacketsRx` / `mqttPacketsTx` incremented on successful host MQTT publish and on handled inbound MQTT application messages (device state/availability/command paths the gateway processes). Expose both pairs in status JSON (`zigbeePacketsRx`/`Tx` aliases or keep `packetsRx`/`Tx` as Zigbee and add `mqttPacketsRx`/`Tx`). UI Network labels use Zigbee / MQTT wording.  
   Alternative: one combined counter with a type tag — rejected; Status needs four rows.

4. **Connections semantics**  
   - **Web active**: sum of live console WebSocket clients (`devices` + `consoles` sockets). HTTP request sockets are short-lived and not counted.  
   - **MQTT inbound**: clients currently accepted on the onboard broker (`MqttBroker::connectionCount()`), or `0` when not listening / SERVER TYPE not local.  
   - **MQTT outbound**: `1` when the host MQTT client is connected, else `0` (covers remote mode and the host’s client to the local broker).  
   - **Total inbound** = web active + MQTT inbound.  
   - **Total outbound** = MQTT outbound.  
   Alternative: include raw LWIP PCB count — rejected; too opaque for operators.

5. **Status DOM / CSS placement**  
   Keep two-column grid on the gateway Status section. Column 1: online time, Version, Network, Connections. Column 2: Free memory, MQTT, Devices, WiFi (WiFi last). Narrow breakpoint remains single column.  
   Simple-user account tab (`user.html`): reuse the same Status groups under Theme and Sign out, forced to one column via `.status-stack-single`. Poll `/api/status` while that tab is visible.  
   Alternative: CSS `order` hacks without DOM reorder — rejected; reorder markup to match reading order.

6. **Devices UI rows**  
   Replace Devices/Online/Packets with four rows: Zigbee devices, Zigbee online, MQTT devices, MQTT online. Packet rows move to Network only.

## Risks / Trade-offs

- [MQTT packet definition undercounts broker-only passthrough] → Count only messages the host client publishes or handles; document that Network MQTT packets are gateway-handled, not every broker hop.  
- [Web active ignores plain HTTP] → Accept; operators care about live WS sessions more than brief REST polls.  
- [MQTT group Connections overlaps Connections group] → Keep MQTT’s combined count for continuity; Connections shows the breakdown.  
- [Stopping MQTT from incrementing Zigbee SPI counters changes existing totals] → Intentional; Network shows the correct split after flash.

## Migration Plan

Flash host (and matching LittleFS `index.html` / `all.css`) at build `3.6.04`. No EEPROM/settings version bump. Rollback: previous image; status JSON is additive so older UI ignores new keys.

## Open Questions

None that block implementation. Connection and MQTT packet definitions above are the recorded assumptions for apply.
