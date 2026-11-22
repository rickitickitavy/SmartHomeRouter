## Why

On **ESP32-P4 only**, about once every 1–2 days the router web console stops answering while ICMP ping, MQTT (including MQTT LED blink on receive), and Zigbee keep working. That pattern matches a stale AsyncWebServer listen socket after a Wi‑Fi blip, not a full host hang. P4 STA runs over **ESP-Hosted C6**; the host may restore IP/MQTT after a Hosted/C6 reinit without the Arduino STA disconnect / GOT_IP edges that today arm `WebConsole::rebind()`. Investigate and close that P4-only gap.

## What Changes

- **P4 host only:** when Hosted Wi‑Fi / transport recovers (or STA is usable again after a Hosted-side reinit that did not fire the usual STA events), feed the **existing** `requestStaWebRebind` → `runPendingStaWebRebind` → `WebConsole::rebind()` path so port 80 answers again on the current STA address.
- Keep rebind on the Arduino `loop` / `WiFiController::update()` path (not inside `WiFi.onEvent`).
- Add clear P4 logs when Hosted recovery arms rebind vs when a normal STA GOT_IP path does, so field failures can be confirmed or ruled out.
- Do **not** change MQTT, Zigbee/SPI, SoftAP product rules, S3 rebind behavior, or invent a standing “probe port 80 every N minutes” product loop unless Hosted events prove unreliable after the primary fix attempt.

## Capabilities

### New Capabilities

(none)

### Modified Capabilities

- `web-console`: P4 STA console MUST recover after Hosted/C6 Wi‑Fi recovery even when ICMP and MQTT already work and classic STA disconnect/GOT_IP edges were missed.

## Impact

- Host firmware only (`BOARD_HOST_P4` paths in `WiFiController` / `main` / Hosted hooks as needed). Slave unchanged.
- Versions: host build **4.5.20** (propose bump); slave remains **4.2.18**.
- Operators on P4: console should become reachable again after Hosted recovery without a host reboot; MQTT/Zigbee unchanged.
