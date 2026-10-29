# Tasks

## 1. README product description

- [x] 1.1 Replace README H1 and lead with Mini-HA router framing (LAN hub, not WAN IP router) — verify: opening paragraphs no longer sell a Zigbee2MQTT-only gateway
- [x] 1.2 Add a concise “What it does” capability overview (dual-chip, Wi-Fi, console/RBAC, devices, MQTT modes, automations, control consoles, joined OTA, userData) — verify: each major shipped area appears once
- [x] 1.3 Keep hardware/build/MQTT topic tables; note SoftAP default name `z2m-gateway` remains a network name, not the product title — verify: Wi-Fi table still documents default BSSID

## 2. Operator console branding

- [x] 2.1 Set gateway console `<title>` to Mini-HA router — verify: `data/index.html` document title
- [x] 2.2 Set sidebar brand heading to Mini-HA router — verify: `.sidebar-title` text in the main shell
- [x] 2.3 Bump `FIRMWARE_VERSION` build on apply if propose bump is already consumed — verify: `include/Defines.h`

## 3. OpenSpec Purpose copy

- [x] 3.1 Update `openspec/specs/web-console/spec.md` Purpose to Mini-HA router + full console surface — verify: Purpose no longer says only “Zigbee–MQTT gateway”
- [x] 3.2 Update other capability Purpose lines that market a narrow gateway (`mqtt-broker` at minimum) — verify: Purpose text matches Mini-HA router framing without changing requirement SHALL bodies
- [x] 3.3 Sync this change’s web-console delta (product identity requirement) into main specs when archiving — verify: ADDED requirement present under `## Requirements`
