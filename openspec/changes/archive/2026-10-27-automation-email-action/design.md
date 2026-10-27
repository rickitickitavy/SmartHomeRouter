# Design

## Context

See proposal.md — Why. Today Network has WiFi / MQTT / ZigBee tabs backed by `GlobalSettings` + `/api/mqtt` (and peers). Automations only support `device` and `automation` action targets (`AutomationAction` / `AutomationEngine::applyAction`). Host has PSRAM (`BOARD_HAS_PSRAM`); automation strings currently use internal `malloc` via `AutomationStore::duplicateBoundedString`. No SMTP dependency exists in `platformio.ini`.

## Goals / Non-Goals

**Goals:**

- Full SMTP account group in settings + Network → E-mail UI + HTTP API.
- Email automation action with live device/attribute values appended as `title: value` lines.
- Non-blocking send from the automation path; email-action config in PSRAM only.
- Skip + warn when account not configured; continue later actions.

**Non-Goals:**

- Inbound IMAP/POP, mailing lists, HTML templates, attachments, or multiple SMTP profiles.
- Slave involvement or Zigbee-path email.
- Changing device/automation action semantics or trigger/condition rules.
- Guaranteed delivery / retries beyond what the SMTP client provides for a single send attempt (log failure; optional single retry can be added later without spec change if kept transparent).

## Decisions

1. **Email settings live in `GlobalSettings` (EEPROM), version bump**  
   Add `EmailSettings` beside Wi‑Fi/MQTT/Zigbee; bump `GLOBAL_CURRENT_SETTINGS_VERSION` (6 → 7) with an upgrade path that zero-fills email defaults. Keep `sizeof(GlobalSettings) ≤ 4096` and reserved/align rules. Include `email` in settings export/restore.  
   *Alternatives considered:* LittleFS-only file (easier size, inconsistent with other Network groups); stuffing into `reserved[256]` (too small for max fields).

2. **Maximum account fields (fixed struct)**  
   Persist: `host`, `port`, `encryption` (`none` | `starttls` | `ssl`), `authEnabled`, `username`, `password`, `fromAddress`, `fromName`, `replyTo`, `ehloHostname`, `connectTimeoutMs`, `sendTimeoutMs`. Defaults: port 587, encryption `starttls`, auth on, timeouts sensible (e.g. 10–30 s). “Configured” gate matches the outbound-email spec (host + port + From; credentials only if auth enabled).

3. **SMTP via ESP-Mail-Client, created only for each send**  
   Add `mobizt/ESP-Mail-Client` for host env only. Do **not** instantiate the SMTP client (or a permanent SMTP worker) at boot. `enqueue(receivers, subject, body)` copies the job to PSRAM, starts a **one-shot** FreeRTOS task, and returns immediately. That task constructs `SMTPSession` / config / message, sends, closes the session, deletes those objects, frees the job, and exits. Automation `applyAction` only builds body + enqueue (or warn if unconfigured / task create fails).  
   Pin the one-shot to **CPU1** at priority below `loopTask`, and pin `async_tcp` to **CPU0**, so blocking TLS cannot starve MQTT/web or trip IDLE0/`async_tcp` TWDT. Serialize overlapping sends with a mutex. Do **not** disable core WDTs around SMTP.  
   *Alternatives:* Permanent worker + queue from boot (rejected — keeps client lifetime tied to uptime); inline SMTP in `applyAction` (blocks loop — rejected); SMTP on CPU0 with IDLE0 WDT paused (rejected — freezes router networking).

4. **Action model: third target `e-mail`**  
   Extend `AutomationActionTarget` with `AutomationTargetEmail`. Email-specific fields (receivers, theme, message, param list ≤8) hang off the action; device/automation fields unused. JSON: `"target":"e-mail"`, `receivers`, `theme`, `message`, `parameters:[{ieee, channel?, param|cluster+attrId, title}]`. No persisted static `value` — live read at run via the same status resolution path conditions use.

5. **Entire email-action config in PSRAM**  
   Allocate the email `AutomationAction` node, its receivers/theme/message strings, and parameter structs/strings with `ps_malloc` / `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`. Device and automation actions keep the existing internal-heap path. Free paths must match. Parse failure if PSRAM alloc fails — never fall back to internal heap for email-action config.

6. **UI mirrors MQTT Network card + action target select**  
   Network tab button `E-mail`, panel `network-tab-email`, load/save/reset like MQTT. Automation target `<option value="e-mail">`, show email fields; parameter rows reuse device/attribute pickers from Trigger (status bindings), plus title; cap Add at 8.

7. **Body assembly**  
   `message` + `\n` (if message non-empty) + for each param: `title` + `: ` + liveValue + `\n`. Missing live value → empty after `: `. Receivers split on `;`, trim whitespace, skip empty tokens.

## Risks / Trade-offs

- [SMTP lib flash/RAM size] → Host-only `lib_deps` / build filter; keep worker stack modest; prefer PSRAM for job body.  
- [EEPROM layout / version upgrade bugs] → Explicit v6→v7 upgrade mirroring prior mqtt born-topic upgrade; fixture log for sizeof/offsets.  
- [Queue full / STA down] → Log warning, drop or refuse enqueue; do not stall automations.  
- [Password in export JSON] → Same as MQTT password today (admin export includes secrets).  
- [Fragile OTA zones] → No edits under `slave-spi-ota` / `firmware-update-process`.

## Migration Plan

1. Ship settings v7 with empty email defaults; existing v6 boards upgrade in place.  
2. Automations without email actions unchanged.  
3. Rollback: flash prior firmware; email JSON fields ignored by old parser; email EEPROM region unused until wiped by older layout rules (document: re-save Network groups after downgrade if needed).

## Open Questions

None that block specs or tasks; library package exact version pinned at apply time from PlatformIO registry.
