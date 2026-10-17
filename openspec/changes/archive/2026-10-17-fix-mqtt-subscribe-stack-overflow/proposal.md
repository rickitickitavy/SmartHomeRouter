# Proposal

## Why

After a remote MQTT connect, the host panics with a stack-canary watchpoint (`IDLE1`, corrupted backtrace) while flooding dozens of device topic subscriptions and then handling server-born / Zigbee status collect. The operator loses the gateway until reboot; this must not happen on a normal device map size.

## What Changes

- Pace remote MQTT session attach so device topic subscribe/unsubscribe work is not done as one blocking burst on the Arduino loop task.
- Keep the final subscribed set the same as today (born topic, MQTT device state/availability, Zigbee command/state topics gated by `publishToMqtt`).
- Reduce stack/heap pressure during attach (fewer large temporaries / log concatenations on the hot path).
- Ensure server-born-triggered Zigbee status collect does not run in the same tight call stack as the full subscribe burst.
- Leave observable MQTT topic membership and born/status-collect behavior unchanged aside from pacing/ordering within a short settle window.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `mqtt-device-topics`: Remote MQTT connect/session attach MUST subscribe required device topics without overflowing host stacks or rebooting; work MAY be spread across loop ticks while still completing promptly.
- `web-console`: Extend the existing “console stays usable during MQTT reconnect” requirement so post-connect subscription attach also must not freeze or crash the host service loop.

## Impact

- Host: `MqttClient` (`finishRemoteSessionAttach`, `subscribeDeviceCommands` / `keepOrSubscribe`), possibly `main.cpp` server-born → status-collect timing, optional loop-stack / build flags in `platformio.ini`.
- No intentional MQTT topic schema, settings API, or web UI form changes.
- Firmware build version bumped to `3.5.07` for this propose.
