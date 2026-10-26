# Proposal

## Why

Simple users who check **Remember me** lose their session when the gateway reboots, because sessions live only in RAM. Household tablets and phones should stay signed in across reboot without writing a session table to LittleFS or any other ROM.

## What Changes

- When a **simple user** logs in with **Remember me** checked, the session MUST remain valid after a host reboot for the existing remember lifetime (`AUTH_REMEMBER_MS`, 30 days), as long as the browser still sends the cookie and the user is still allowed.
- Durable proof MUST live in the browser cookie (and be revalidated on the host after boot). The host MUST NOT persist remember-me / session slots to LittleFS, NVS, EEPROM, or any other ROM.
- Without **Remember me**, behavior after reboot stays as today (session lost; login form again).
- `isAdmin` continues to ignore Remember me (10-minute idle). Privileged non-admin operator sessions are unchanged by this change (no new reboot-survival for them unless they are simple users).
- Password change, block, or delete of that user MUST invalidate any outstanding remember cookie for that user.
- Explicit logout MUST clear the cookie and end the live session as today.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `console-users`: Extend Remember me so a simple-user remember session survives host reboot without ROM-backed session storage; keep admin ignore-Remember-me and idle rules.

## Impact

- `UserStore` session create/validate/logout paths (`startSession`, `sessionUser`, cookie token format).
- `WebConsole` login cookie `Max-Age` / Set-Cookie for remember-me simple users.
- Possibly `data/user.html` / `data/index.html` only if cookie handling needs adjustment (likely none).
- Specs: `openspec/specs/console-users/spec.md`.
- Firmware build bumped to `3.5.62` at propose time.
