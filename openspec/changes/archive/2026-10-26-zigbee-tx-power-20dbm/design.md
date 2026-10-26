# Design

## Context

Zigbee runs only on the ESP32-C6 slave (`ZigbeeCoordinator::begin` → `Zigbee.begin(ZIGBEE_COORDINATOR)`). There is no call to `esp_zb_set_tx_power` / `esp_zb_get_tx_power` today. Espressif ESP-Zigbee API documents TX power in dB ranging from the IEEE802154 TXPOWER min/max; ESP32-C6 802.15.4 max is **+20 dBm**. ZBOSS default `ZB_MAC_DEFAULT_TX_POWER_24_GHZ` is already +20; this change makes the product contract explicit.

`FIRMWARE_VERSION` propose bump: `3.6.02`.

## Goals / Non-Goals

**Goals:**

- Explicitly set Zigbee TX power to **+20 dBm** once the coordinator has started successfully.
- Confirm via `esp_zb_get_tx_power` and an info log line.

**Non-Goals:**

- Configurable TX power in settings / web UI.
- Changing RX sensitivity (not available in software).
- Host Wi‑Fi TX power (unrelated).
- Re-applying TX power on channel change or permit-join (set once at begin is enough unless stack resets power).

## Decisions

1. **Call site** — Immediately after successful `Zigbee.begin` in `ZigbeeCoordinator::begin`, before returning `true`.  
   *Why:* Radio stack is up; API is valid. Failures of begin leave power unset (no radio).

2. **API** — Use `esp_zb_set_tx_power(20)` and `esp_zb_get_tx_power` from `esp_zigbee_core.h` (already linked via ZCZR slave build).  
   *Alt considered:* Arduino `Zigbee` wrapper helper — none found in this tree; use ESP-IDF Zigbee API directly.

3. **Constant** — Hard-code `20` (or a local `static constexpr int8_t kZigbeeTxPowerDbm = 20`). No EEPROM/settings field in this change.

4. **Failure** — If set/get is a void API (as in headers), treat set as best-effort and still log the get value. Do not fail `begin` solely because power read-back differs (PHY may clamp); log a warning if read-back is not 20.

## Risks / Trade-offs

- [Risk] Regional regulatory limits may prefer lower TX → Mitigation: product requests max; future settings can lower it.
- [Risk] Stack later overwrites TX power → Mitigation: none in this change; reopen if observed.

## Migration Plan

- Rebuild/flash slave (joined firmware as usual). No persisted settings change.
- Rollback: remove the `esp_zb_set_tx_power` call.

## Open Questions

- None.
