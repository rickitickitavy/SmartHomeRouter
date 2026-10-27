# Proposal

## Why

Automations can already command devices and control other automations, but operators have no way to notify people by email when a rule fires (leak, offline, alarm). The gateway already has Network settings for Wi‑Fi/MQTT/ZigBee; outbound SMTP and an email action close that gap without keeping rare email action payloads on the scarce internal heap.

## What Changes

- Add a Network → **E-mail** subtab with an **E-Mail** settings card for a full outbound SMTP account (host, port, encryption, auth, credentials, From / display name, Reply-To, EHLO hostname, timeouts, and related send options).
- Persist those account settings on the host (EEPROM/settings group + `GET`/`POST` API), same life cycle as MQTT/ZigBee groups.
- Add automation action target/type **e-mail** with: receivers (`;`-separated), theme (subject), message body text, and up to **8** additional parameters. Each parameter stores **device**, **attribute** (known param / binding used to read live status), and **title** (human label). At send time the host reads the live value from that device/attribute; the body appends one line per parameter as `title: <value>` (device name is not printed).
- Send mail **asynchronously** so automation action execution does not block the host loop on SMTP.
- Keep email-action configuration (strings and parameter list) in **PSRAM**, not internal RAM, because this action type is rare.
- If the email account is **not configured**, skip the email action and log a **warning** (do not fail the whole automation run for other actions).
- `FIRMWARE_VERSION` propose bump `3.7.14` → `3.7.15` (apply bumps again when source lands).

## Capabilities

### New Capabilities

- `outbound-email`: Host SMTP account settings (maximum practical fields), “configured” gate, and async outbound send used by automation email actions.

### Modified Capabilities

- `automations`: New **e-mail** action type (receivers, theme, message, ≤8 device/attribute/title parameters); live value resolution into body lines; PSRAM-backed storage for this action’s config; skip-with-warning when SMTP account is not configured.
- `web-console`: Network → **E-mail** tab and E-Mail card; automation Actions editor support for **e-mail** (receivers, theme, message, parameter rows).

## Impact

- Host settings: extend `GlobalSettings` / `SettingsManager` with an email group; WebConsole `/api/email` (or equivalent) load/save; `data/index.html` Network tab + automation action UI.
- Host runtime: SMTP client dependency (async), email sender service, `AutomationStore` / `AutomationEngine` parse-run-serialize for email actions, PSRAM allocators for email-action fields.
- LittleFS automations JSON grows with email action objects; import/export/list must round-trip the new fields.
- No change to fragile OTA / slave-SPI-OTA zones.
- Slave unchanged (email is host-only).
