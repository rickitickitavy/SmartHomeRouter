# Design

## Context

ESP32 Arduino (pioarduino) ships `CONFIG_LWIP_MAX_ACTIVE_TCP=16` (and matching `CONFIG_LWIP_MAX_SOCKETS` / listening TCP) in the prebuilt SDK for esp32-s3. AsyncWebServer does not impose its own HTTP client count; it uses LWIP PCBs. This host runs HTTP + MQTT broker (+ optional remote MQTT client) on that shared pool. `MQTT_BROKER_CLIENT_MAX` is already 24 in `Defines.h`, which exceeds today’s LWIP 16.

## Goals / Non-Goals

**Goals:**
- Host build: effective `CONFIG_LWIP_MAX_ACTIVE_TCP >= 48` (set to **48**).
- Align `CONFIG_LWIP_MAX_SOCKETS` and `CONFIG_LWIP_MAX_LISTENING_TCP` to **32**.
- Raise AsyncTCP event queue if needed so more concurrent connections do not starve work (`CONFIG_ASYNC_TCP_QUEUE_SIZE=128` per ESP32Async IncreaseMaxSockets example).
- Verify after build that the host sdkconfig / compile definitions reflect the new values.

**Non-Goals:**
- Changing slave Zigbee firmware TCP limits (no AsyncWebServer / broker on slave).
- Ceiling set to **48** (satisfies the ≥32 floor).
- Changing application MQTT accept policy beyond fitting under the new LWIP ceiling.
- Reworking WebSocket `DEFAULT_MAX_WS_CLIENTS` unless something blocks at apply time.

## Decisions

1. **Host-only `custom_sdkconfig` on `esp32-s3-host`**
   - Use pioarduino `custom_sdkconfig` for:
     - `CONFIG_LWIP_MAX_ACTIVE_TCP=48`
     - `CONFIG_LWIP_MAX_SOCKETS=48`
     - `CONFIG_LWIP_MAX_LISTENING_TCP=48`
   - Alternative rejected: patching framework `sdkconfig.h` by hand (not reproducible).

2. **AsyncTCP queue via `build_flags`**
   - Add `-DCONFIG_ASYNC_TCP_QUEUE_SIZE=128` on the host env (library default is 64).
   - Keeps HTTP accept/event processing healthier under more concurrent clients.

3. **Leave `MQTT_BROKER_CLIENT_MAX` at 24**
   - Fits under 32; leaves headroom for HTTP/console sockets.
   - If apply finds broker still hard-capped by something else, fix only what blocks the requirement.

## Risks / Trade-offs

- **RAM**: Extra TCP PCBs use more LWIP RAM; host has PSRAM and 16MB flash — acceptable at 48.
- **Rebuild cost**: `custom_sdkconfig` may force a longer first rebuild of Arduino/IDF libs for that env.
- **Still a shared pool**: many MQTT clients can still starve HTTP; operators must leave headroom for the console.
