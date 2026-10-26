# Proposal

## Why

Status already shows device and traffic totals in one Devices group, but operators cannot tell Zigbee from MQTT load or see connection pressure separately from Wi‑Fi. Reorganizing Status into clearer Network and Connections groups, with transport-split device and packet counters, makes live gateway health readable at a glance.

## What Changes

- Reposition Status groups so **Devices** sits in the **second column after MQTT** (Free memory stays at the top of column 2) and **WiFi** is last in column 2.
- Add a **Network** group before Connections with Zigbee and MQTT packets sent/received (four rows).
- Remove **Packets sent** and **Packets received** from the **Devices** group.
- Add a **Connections** group after Network with: Web active connections, MQTT inbound connections, MQTT outbound connections, total inbound connections, total outbound connections.
- Split **Devices** counters into Zigbee vs MQTT registered and online counts (no combined Devices/Online rows).
- Show the same Status summary on a simple user’s account tab (after Theme and Sign out), forced to a single column.
- Extend `GET /api/status` with the new fields needed by those groups (additive JSON).
- **BREAKING** (UI only): Status layout and Devices rows change; operators no longer see combined device/packet totals on Devices.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: Status group order/placement, Devices contents, new Network and Connections groups, and status API fields that feed them.

## Impact

- Host status JSON (`hostGatewayStatusJson` in `src/main.cpp`) and Status UI (`data/index.html`, `data/css/all.css`).
- Packet/connection counters on host MQTT client/broker paths and web console sockets as needed for Network and Connections.
- Firmware version build bump to `3.6.04` (propose).
- LittleFS console assets must match the host image after flash.
