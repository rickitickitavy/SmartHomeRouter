# Proposal

## Why

The host ESP32-S3 firmware currently uses the Arduino/LWIP default of **16** active TCP connections for the whole chip. That pool is shared by AsyncWebServer, WebSockets, the local MQTT broker, and any outbound MQTT client. Operators need room for more concurrent console and MQTT clients; the floor is **at least 32**.

## What Changes

- Raise the host LWIP active TCP PCB limit to **48** (and align related socket / listening TCP caps so the raise is usable).
- Adjust AsyncTCP queue sizing on the host so more concurrent HTTP/TCP activity does not starve the async event queue (follow ESP32Async guidance when raising sockets).
- Document the shared-pool behavior: web and MQTT compete for the same TCP limit.
- No change to MQTT application `MQTT_BROKER_CLIENT_MAX` unless implementation discovers it must stay ≤ the new LWIP cap (today it is 24, which fits under 32).

## Capabilities

### New Capabilities
- `host-network`: Host IP stack capacity for concurrent TCP sessions used by HTTP/console and MQTT on the gateway.

### Modified Capabilities
- (none)

## Impact

- `platformio.ini` host env (`esp32-s3-host`): `custom_sdkconfig` and/or AsyncTCP `-D` build flags.
- Rebuild of host firmware (slave Zigbee image unchanged unless we deliberately mirror flags; default: host only).
- Slightly higher static LWIP RAM for extra PCBs on the S3 (acceptable with PSRAM/N16R8 host).
- Firmware version build bump already applied for this propose (`3.5.59`).
