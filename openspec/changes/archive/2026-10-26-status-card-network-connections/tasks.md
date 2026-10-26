# Tasks

## 1. Status API: device and packet fields

- [x] 1.1 Count Zigbee vs MQTT registered and online devices in `hostGatewayStatusJson` and expose split fields (keep legacy totals); verify `/api/status` returns both pairs with known mixed transports
- [x] 1.2 Keep Zigbee SPI `packetsRx`/`packetsTx` Zigbee-only (stop MQTT paths from incrementing them) and verify Zigbee counters no longer move on MQTT availability alone
- [x] 1.3 Add MQTT packet sent/received counters on host MQTT publish and handled inbound messages; expose them on `/api/status` and verify Network-bound fields increase after one publish and one handled inbound message

## 2. Status API: connections

- [x] 2.1 Expose web active (devices + consoles WebSocket counts), MQTT inbound, MQTT outbound, total inbound, and total outbound on `/api/status` per design.md; verify values match open WS clients and broker/client state

## 3. Status UI layout and groups

- [x] 3.1 Reorder Status markup/CSS so column 1 is online time → Version → Network → Connections → WiFi and column 2 is Free memory → MQTT → Devices; verify wide and narrow layouts match the spec scenarios
- [x] 3.2 Replace Devices rows with Zigbee/MQTT devices and online counts; remove Packets sent/received from Devices; wire `applyGatewayStatus` to the new fields and verify the Devices group matches the status JSON
- [x] 3.3 Add Network group rows (Zigbee/MQTT packets sent and received) and bind them; verify counters render from `/api/status`
- [x] 3.4 Add Connections group rows (web active, MQTT inbound/outbound, totals) and bind them; verify totals equal the stated sums

## 4. Integration check

- [x] 4.1 Open Status with mixed Zigbee and MQTT devices and some traffic/connections; verify layout, Devices split, Network packets, and Connections breakdown against the delta scenarios

## Workflow follow-up

- Archive the change after review when implementation is complete.
