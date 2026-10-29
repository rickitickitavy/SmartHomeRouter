# Proposal

## Why

Public and in-console copy still frames the product as a small Zigbee–MQTT gateway (README title, browser title `z2m-gateway`, sidebar “Gateway”). That understates the shipped stack: dual-chip radio host, local/remote MQTT, device registry, automations, role-based web console, and household control consoles. Operators and contributors need a product description that matches the full-stack smart home router.

## What Changes

- Rewrite the README lead so the product is a **full-stack Mini-HA router**, with a capability overview (not only Zigbee↔MQTT bridging).
- Update operator-visible branding in the gateway console (document title and sidebar brand) to **Mini-HA router**.
- Refresh OpenSpec capability **Purpose** blurbs that still describe a narrow “Zigbee–MQTT gateway” so they match the Mini-HA router framing (without renaming technical APIs or Wi-Fi default SSID).
- Keep wire identifiers that would break installs: default Wi-Fi **BSSID** `z2m-gateway`, MQTT base topic `z2m`, and internal names like gateway status API.

## Capabilities

### New Capabilities

### Modified Capabilities

- `web-console`: Operator-facing product identity (title/sidebar) and Purpose text describe a Mini-HA router with the console’s full surface area.

## Impact

- `README.md` (primary product description).
- `data/index.html` browser `<title>` and sidebar brand string.
- Selected `openspec/specs/*/spec.md` Purpose paragraphs (apply-time direct edits).
- Firmware build bump for this propose cycle.
- No change to MQTT topic layout, default SoftAP name, SPI protocol, or RBAC rules.
