# Tasks

## 1. Remember cookie format and crypto

- [x] 1.1 Define a distinguishable `z2m_session` remember-proof encoding (user name, wall-clock expiry, MAC over password-hash material) in `UserStore` / `Defines.h` and verify opaque 32-hex RAM tokens still parse as before
- [x] 1.2 Implement issue + verify helpers (HMAC or equivalent) keyed from the user’s stored password hash/salt and verify a known vector accepts a valid proof and rejects a tampered or wrong-hash proof

## 2. Login, session lookup, logout

- [x] 2.1 On simple-user login with Remember me, issue the remember-proof cookie with `Max-Age` = `AUTH_REMEMBER_MS` (keep admin ignore-Remember-me and non-simple long-idle RAM behavior) and verify Set-Cookie from login is not a plain 32-hex-only token for that path
- [x] 2.2 In `sessionUser`, if no RAM slot matches, verify remember proof, reject blocked/missing/expired/wrong-hash, rehydrate a RAM `SessionSlot`, and verify a post-reboot cookie (simulated empty slots) authenticates the same simple user
- [x] 2.3 Ensure logout clears the cookie and any RAM slot for both token styles, and verify a cleared cookie is rejected
- [x] 2.4 On password change (and block/delete), ensure old remember proofs fail verification and verify an old cookie is rejected after `setPassword` / block / delete

## 3. Firmware version and smoke checks

- [x] 3.1 Bump `FIRMWARE_VERSION` build by 1 on apply (propose already set `3.5.62`) and verify the string in `Defines.h` matches the apply bump
- [x] 3.2 Smoke: simple user + Remember me → reboot → control page still signed in; same user without Remember me → reboot → login form; confirm no new LittleFS/NVS session file is written

## Workflow follow-up

- Archive the change after review and implementation are complete.
