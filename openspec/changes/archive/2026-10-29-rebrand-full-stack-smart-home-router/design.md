# Design

## Context

README opens with “ESP32 Zigbee–MQTT gateway” and a thin bridge narrative. The console uses `<title>z2m-gateway</title>` and sidebar `Gateway`. Spec Purpose lines for `web-console` and `mqtt-broker` still say “gateway” in the product sense. The firmware already implements a much larger surface: dual-chip host/slave, Wi-Fi, MQTT (remote or local broker), device registry (Zigbee + MQTT + userData), automations, RBAC users, control consoles with gauges/icons/composite layouts, joined OTA, and SPI orchestration.

**Assumptions (recorded for apply):**
- Display product name: **Mini-HA router** (exact operator-facing string).
- Repo directory / PlatformIO project id may remain `zigbee2mqtt`; no git remote rename in this change.
- Do **not** change default SoftAP/STA BSSID `z2m-gateway` or MQTT base `z2m`.

## Goals / Non-Goals

**Goals:**
- README lead + capability overview matching shipped features.
- Operator console title and sidebar brand use Mini-HA router.
- Update narrow gateway-centric Purpose blurbs on affected OpenSpec capabilities during apply.

**Non-Goals:**
- Renaming C++ symbols, HTTP routes, or MQTT bridge topic paths.
- Changing default Wi-Fi SSID or MQTT base topic.
- Redesigning the console visual theme.
- Rewriting every occurrence of the word “gateway” in requirements (host/slave “gateway” as device role stays fine).

## Decisions

1. **README structure** — Keep hardware/build sections; replace the H1 and opening paragraphs with a Mini-HA router product statement and a “What it does” bullet list covering: dual-chip Zigbee coordinator + Wi-Fi host, web console + RBAC, device registry, MQTT broker modes, automations, household control consoles, joined firmware update, user-data devices.
2. **UI chrome only** — Change `document.title` and `.sidebar-title` text; leave internal JS names like `refreshGatewayStatus` alone.
3. **Purpose edits** — During apply, edit Purpose on `web-console` and `mqtt-broker` (and any other Purpose that markets a “Zigbee–MQTT gateway” only) to Mini-HA router framing; leave requirement bodies intact unless they hardcode the old product title.
4. **Simple-user page** — `user.html` title “Control consoles” may stay (role-specific); optional one-line product subtitle is out of scope unless apply finds an existing brand slot.

## Risks / Trade-offs

- Operators who bookmarked the mental model “z2m-gateway” may not notice SoftAP SSID is unchanged — call that out in README Wi-Fi table.
- Over-claiming “router” (no WAN routing) — README MUST clarify it is a **LAN Mini-HA router / hub** (Wi-Fi AP/STA + Zigbee + MQTT + automation), not a WAN IP router.
