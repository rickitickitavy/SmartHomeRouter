# Proposal

## Why

On the simple-user page, the Status card flickers/blinks while open, unlike the operator (base) Status view. Simple-user console refreshes currently rebuild the account/Status DOM and re-drive `/api/status` differently from the operator Status path. Operators expect the same Status refresh algorithm on both pages, including calmer host web-activity LED behavior while Status is watched.

## What Changes

- Make simple-user Status refresh follow the **same algorithm** as operator Status: poll `/api/status` on a fixed interval only while Status is the visible surface, and update values **in place** via the same apply path.
- Stop tearing down / recreating the simple-user Status card on unrelated console live updates (device/automation control refreshes) while the account/Status tab is open.
- Keep start/stop Status polling tied to Status visibility (operator Status section; simple-user account/Status tab), matching base-user lifecycle.
- No change to Status field set/layout content beyond eliminating the blink/rebuild behavior.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: Simple-user Status refresh MUST use the same poll + in-place update algorithm as operator Status; Status DOM MUST NOT rebuild on every console control refresh while Status is visible.

## Impact

- `data/user.html` Status/account tab rendering and console refresh path; keep behavioral parity with `data/index.html` Status polling.
- Observable side effect: host LED3 (web request activity / request-hold pool) SHOULD stop looking continuously active solely because of Status-card rebuild churn while viewing Status.
- Host firmware build bump for propose; no slave source changes; no new persisted settings (global export unchanged).
