# Design

## Context

See proposal.md — Why. Today `UserStore` keeps up to `AUTH_SESSION_MAX` RAM slots keyed by a random 16-byte token in cookie `z2m_session`. Login with Remember me (or a simple user’s long-idle path) sets cookie `Max-Age` to `AUTH_REMEMBER_MS` (30 days), but after reboot the RAM table is empty so the cookie fails. ADR 0005: `isAdmin` ignores Remember me (10-minute idle). Constraint from this change: no LittleFS / NVS / EEPROM / other ROM for session or remember-me records. User accounts may continue to load from the existing users file.

## Goals / Non-Goals

**Goals:**

- Stateless (or cookie-carried) durable proof for **simple users** who checked Remember me, valid across host reboot for `AUTH_REMEMBER_MS`.
- Rehydrate a normal RAM session after successful cookie validation so idle/touch/logout behavior stays familiar.
- Invalidate remember proof when that user’s password hash changes, or the user is blocked/deleted.

**Non-Goals:**

- Persisting a session table on flash.
- Changing admin 10-minute idle / ignore-Remember-me.
- Adding host-reboot survival for privileged non-admin operators (unchanged).
- Changing HTTP Basic policy, MQTT/CLI auth, or users-table persistence.
- Server-side global logout of all devices beyond password change / block / delete / explicit cookie clear.

## Decisions

### 1. Cookie carries a signed remember payload (not a ROM session index)

**Choice:** For simple-user + Remember me, put a self-contained proof in the cookie (or a distinguishable cookie value): user name, expiry (wall-clock), optional nonce, and an HMAC (or equivalent MAC) keyed from material already available after users load (password hash/salt). On each request, if no RAM slot matches, try to verify the remember proof, then create a RAM slot for the remainder of the remember window.

**Why not flash session file:** Forbidden by the request.

**Why not NVS token list:** Same ROM constraint.

**Why not store only a random token and rebuild nothing:** After reboot there is nothing to look up.

**Alternatives considered:** Browser-stored username/password (rejected — not HTTP Basic, worse UX/security). Compile-time device HMAC secret alone (works, but tying MAC to password hash also invalidates on password change without a version field).

### 2. Scope reboot survival to simple users with Remember me checked

Match the stated product rule. Today simple users already get long idle even without the checkbox while the host stays up; reboot survival requires the checkbox. Privileged non-admin + Remember me keep browser-restart long idle only (RAM), no new reboot path.

### 3. Wall-clock expiry for remember proof

RAM idle uses `millis()`. Remember proof MUST use absolute time for the 30-day bound so reboot does not reset the lifetime. Prefer `time(nullptr)` / SNTP when available; if wall clock is unset (pre-SNTP), fall back to issuing remember cookies that still work for the uptime-based window within one boot, or require clock for the durable path — prefer: use epoch if `time` looks sane, else still issue signed cookie with expiry = now_estimate + AUTH_REMEMBER_MS using a boot-relative baseline only for that boot (document that full cross-reboot guarantee needs a sane clock; if the project already sets time via SNTP/STA, rely on that).

**Assumption recorded:** Host obtains usable wall time in normal STA deployments (existing network stack). If epoch is clearly invalid at login, still set Max-Age and signed expiry from a monotonic offset only for the current boot (cross-reboot may fail until clock is set) — acceptable edge case, not a second product mode.

### 4. Cookie format compatibility

Keep cookie name `z2m_session`. Use a value prefix or length/structure that distinguishes opaque RAM tokens (32 hex chars today) from remember payloads so `sessionUser` can branch: try RAM hex token first; else parse/verify remember proof. Logout clears cookie (`Max-Age=0`) and clears any RAM slot.

### 5. Rehydrate into existing SessionSlot

After remember verify: allocate/reuse a RAM slot with `maxIdleMs = AUTH_REMEMBER_MS` (or remaining lifetime), `userName` from proof, new or reused random token **or** keep presenting the same remember cookie value. Prefer keeping the remember cookie value stable across reboot so the browser does not need a Set-Cookie on every first post-boot hit; optionally rotate to a short RAM token via Set-Cookie on first successful verify (nice-to-have, not required).

## Risks / Trade-offs

- [Stolen cookie until expiry] → Mitigate with HttpOnly cookie, HMAC bound to password hash, HTTPS when available; password change invalidates.
- [No wall clock after reboot] → Mitigate with SNTP/STA time; document edge case if epoch invalid.
- [Cookie size] → Keep payload compact (name + expiry + short MAC).
- [Logic split RAM vs remember] → Single `sessionUser` entry path; tests/scenarios for reboot and no-ROM.

## Migration Plan

- No flash schema migration.
- Existing random-token cookies become invalid after reboot as today until users log in again with Remember me (new proof format).
- Rollback: revert to RAM-only tokens; users re-login.

## Open Questions

None blocking; clock edge case handled as assumption above.
