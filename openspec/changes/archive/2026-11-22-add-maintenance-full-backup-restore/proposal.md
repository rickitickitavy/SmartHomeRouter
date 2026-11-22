## Why

Operators need a single **Full backup** / **Full restore** on System → Maintenance that can capture a board’s durable state (settings export contents **plus** scene bake images, and optionally Wi‑Fi, history samples, and the important secrets that normal Export settings omits). Restoring must wipe the board’s owned config first so leftovers cannot orphan against the backup, then reboot.

## What Changes

- Add **Full backup** and **Full restore from backup** at the **top** of the Maintenance card (above Export/Restore settings). **Main role only** (hide/disable on Repeater).
- Full backup downloads **one ZIP**.
- Backup options (defaults **ON**): **Include Wi‑Fi**, **Include historical data**, **Include important passwords**.
- **Important passwords** = secrets normal Export settings does **not** ship in usable form today: at least **e-mail SMTP password**, and **Wi‑Fi PSK** when Wi‑Fi is included. Camera ONVIF passwords and other fields already exported in plaintext stay as they are (not double-wrapped). Console user passwords remain hashes only.
- If **Include important passwords** is on, the operator **must** set a passphrase; those secret fields are **encrypted inside the JSON** (ZIP itself is not wholesale-encrypted).
- Scenes in the backup **always** include bake JPEG bytes when present (not metadata-only).
- Full restore **always** clears owned durable data on the board first (devices, users, consoles, automations, cameras, scenes + bake images, interconnect registry, settings groups the backup owns, and history per rules below), then applies the archive.
- Restore UI lets the operator choose whether to import **Wi‑Fi**, **e-mail**, and **historical data** (independent of what the ZIP contains; missing slices follow leave/wipe rules).
- Wrong passphrase: offer **continue without important passwords** (leave current e-mail/Wi‑Fi secrets) rather than hard-fail the whole restore.
- After successful restore apply: **host restarts**. If restored STA cannot join, **switch to SoftAP** (recovery AP) with restored config kept.
- Cross-board / older firmware: **allow with clamps** (same numeric clamp spirit as settings restore). Existing Export/Restore settings stay as-is.

## Capabilities

### New Capabilities

(none)

### Modified Capabilities

- `web-console`: Maintenance Full backup / Full restore ZIP flows, options, wipe semantics, reboot/AP fallback.
- `historical-data`: Full backup may embed history samples; full restore can replace or wipe history.

## Impact

- Host firmware + operator `data/index.html` Maintenance UI. Slave unchanged unless apply discovers a device-store edge that already goes through host restore paths.
- Host build **4.5.22** (propose); slave **4.2.18** unchanged.
- ZIP may be large when history + scene images are included; PSRAM/streaming and Main-only constraints apply.
- Ordinary Export settings remains Wi‑Fi-omitted and e-mail-password-blank; Full backup is the opt-in path for those.
