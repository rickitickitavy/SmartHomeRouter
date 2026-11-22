## Context

Today Maintenance **Export settings** downloads JSON: MQTT/Zigbee/hardware/history retention fields/theme/devices/users/consoles/automations/cameras/scenes (no bake bytes)/interconnect; **omits Wi‑Fi**; **blanks e-mail password**; camera passwords plaintext; user password hashes only; **no history samples**.

Operator wants a separate **Full backup / Full restore** ZIP on the same card with optional Wi‑Fi, history, and important passwords; obligatory scene images; wipe-then-restore; reboot.

## Goals / Non-Goals

**Goals:**

- Main-only Full backup ZIP + Full restore with wipe-first semantics.
- Optional Include Wi‑Fi / history / important passwords (default ON).
- Encrypt only important secret fields in JSON when that option is on; passphrase required then.
- Always embed scene bake images when scenes are present.
- Restore import toggles for Wi‑Fi, e-mail, history; wrong-passphrase continue-without-secrets; reboot; STA fail → AP.

**Non-Goals:**

- Changing ordinary Export/Restore settings behavior.
- Encrypting the entire ZIP or camera passwords already exported plaintext.
- Shipping plaintext console user passwords.
- Repeater Full backup/restore.
- Including firmware OTA packages, Hosted-C6 blob, or volatile logs as required members.

## Decisions

1. **UI placement**  
   Maintenance card top: **Full backup**, **Full restore from backup**, then existing Export/Restore settings. Main only.

2. **ZIP layout (apply may refine names)**  
   - `manifest.json` — version, flags (wifi/history/secretsPresent), created board/role hints.  
   - `settings.json` — same logical slices as settings export, plus Wi‑Fi object when opted in; e-mail password either omitted/blank or ciphertext field when secrets opted in; scenes metadata as today.  
   - `scenes/bakes/<id>.jpg` (or equivalent) — bake files for scenes that have them.  
   - `history/` — opaque or documented sample dump when history opted in (board restores into LittleFS or SD as this host uses).  
   ZIP is not password-zip-encrypted; secrets are field-level ciphertext inside JSON.

3. **Backup toggles (default ON)**  
   - **Include Wi‑Fi** — wifi group fields in `settings.json`.  
   - **Include historical data** — history payload member.  
   - **Include important passwords** — requires passphrase; writes encrypted e-mail password; when Wi‑Fi is also included, writes encrypted Wi‑Fi PSK (both were omitted/unusable in normal export). Camera and other already-exported secrets stay plaintext as in settings export.

4. **Crypto (apply picks library already on host if possible)**  
   Passphrase → KDF → symmetric encrypt of those secret string fields only; store algorithm/kdf/salt/iv metadata next to ciphertext so restore can decrypt. Never log passphrase or plaintext secrets.

5. **Full restore wipe-first**  
   Always remove owned durable entities before apply: devices, users, consoles, automations, cameras, scenes **and** bake files, interconnect paired data as settings restore does, and replace settings groups from the file. Then apply ZIP. No merge-left orphans for those collections.

6. **Restore import toggles**  
   Operator chooses import **Wi‑Fi**, **e-mail**, **historical data** (defaults reflecting what the ZIP contains / ON when present).  
   - Wi‑Fi off or absent in ZIP: **keep current Wi‑Fi** (including PSK).  
   - E-mail off or secrets not decrypted: apply non-secret e-mail fields if present; **keep current e-mail password**.  
   - History off or absent: **wipe history store empty** (operator rule 6).  
   - History on: replace history from ZIP.

7. **Wrong passphrase**  
   Do not abort the whole restore by default: prompt to **restore without important passwords** (leave current e-mail/Wi‑Fi secrets; continue wipe+apply of the rest). Operator may cancel.

8. **Reboot and STA failure**  
   After successful apply, host reboots. On boot, if MODE STA and join fails in the existing join window, switch to SoftAP (existing recovery AP behavior) with restored settings kept.

9. **Compatibility**  
   Allow S3↔P4 and older builds with clamps on bounded numerics; skip unknown members; reject only if ZIP/manifest unreadable.

10. **Orphans explicitly handled**  
    - Scene bake files without scene rows → deleted on wipe; not left on LittleFS.  
    - History series for devices removed by wipe → gone when history wiped/replaced.  
    - Slave device table → use same host device restore/publish path as settings restore so slave is not left with pre-wipe devices.  
    - In-memory caches / live MQTT → cleared by reboot.  
    - Partial upload failure → no wipe until archive validated (validate ZIP + decrypt attempt before destructive wipe).

## Risks / Trade-offs

- **[Risk] Large history ZIP OOMs AsyncWebServer** → Mitigation: stream to temp on SD/LittleFS when possible; Progress UI; Main-only; document size limits if needed at apply.  
- **[Risk] Wipe before apply then crash** → Mitigation: validate archive + passphrase path before wipe; stage on disk; reboot completes consistency.  
- **[Risk] Operator expects ZIP password** → Mitigation: UI copy: passphrase encrypts secrets in JSON, not the ZIP file.  
- **[Risk] Wi‑Fi included without secrets** → Mitigation: restore Wi‑Fi non-secrets only; keep current PSK (same as leave-secrets rule).

## Migration Plan

- Flash host **4.5.22**. No automatic migration of old backups (new format). Ordinary settings JSON restore unchanged.  
- Rollback: previous host image; no settings wipe required for rollback itself.

## Open Questions

None blocking — operator answers recorded in Decisions.
