## 1. Format and crypto

- [x] 1.1 Define ZIP members (`manifest`, `settings.json`, scene bakes, optional `history/`) and version field
- [x] 1.2 Implement passphrase KDF + encrypt/decrypt for e-mail password and Wi‑Fi PSK fields only; never log secrets
- [x] 1.3 Validate archive (and passphrase path) **before** any destructive wipe

## 2. Full backup (host + UI)

- [x] 2.1 Add Main-only Maintenance controls at top: Full backup / Full restore; toggles Include Wi‑Fi, Include historical data, Include important passwords (default ON)
- [x] 2.2 Build settings slice from existing export path; add optional Wi‑Fi; blank/omit important secrets unless opted in
- [x] 2.3 Always pack scene bake JPEGs when present; pack history samples when opted in
- [x] 2.4 Require passphrase when Include important passwords is on; write ciphertext fields into JSON; stream ZIP download

## 3. Full restore (host + UI)

- [x] 3.1 Restore UI: confirm; import toggles Wi‑Fi / e-mail / historical data; passphrase when secrets present
- [x] 3.2 Wrong passphrase → offer continue without important passwords or cancel
- [x] 3.3 Wipe owned collections + scene bake files (+ history wipe when not importing history); apply ZIP; publish devices like settings restore
- [x] 3.4 Leave current Wi‑Fi/e-mail secrets when not imported or not decrypted; replace history when imported
- [x] 3.5 Host reboot after success; STA join failure uses existing SoftAP recovery (no config rollback)

## 4. Compatibility and smoke

- [x] 4.1 Clamp bounded numerics like settings restore; allow S3↔P4/older with skips for unknown members
- [x] 4.2 Keep ordinary Export/Restore settings behavior unchanged
- [x] 4.3 Build host; do not flash unless the human approves
- [x] 4.4 Smoke: defaults-on backup; secrets+passphrase; restore wipe+reboot; skip history wipes; wrong passphrase continue-without-secrets; Repeater hides controls

## 5. Versions

- [x] 5.1 Confirm propose host **4.5.22**; slave unchanged unless apply touches slave sources
