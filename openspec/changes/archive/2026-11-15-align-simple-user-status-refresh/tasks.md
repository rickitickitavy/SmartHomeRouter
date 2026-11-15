# Tasks

## 1. Stop Status card rebuild on console live refresh

- [x] 1.1 Change simple-user console refresh so that while the account/Status tab is visible it does not destroy/recreate the Status DOM (skip full `renderConsoleTabs` Status rebuild or equivalent); verify a devices-live upsert no longer resets Status values to "—"
- [x] 1.2 Ensure entering the account tab still builds Status once and calls `startStatusPolling`, and leaving for a console tab still calls `stopStatusPolling`; verify poll starts/stops with tab visibility like operator Status section

## 2. Match operator Status refresh algorithm

- [x] 2.1 Align simple-user `/api/status` poll cadence and in-place `applyGatewayStatus` updates with operator Status (`index.html`); verify both pages update the same fields without full Status rebuild each tick
- [x] 2.2 Confirm Theme / Sign out / single-column Status content still match the simple-user Status requirement; verify account tab layout unchanged aside from no blink

## 3. Cross-check LED churn and parity

- [x] 3.1 With account/Status open and quiet devices, confirm Status values stay stable between polls and host web-activity LED is not continuously driven by Status-card rebuild churn; verify switching away from Status stops the Status poll
- [x] 3.2 If any Status poll helper is shared or copied, keep `data/index.html` and `data/user.html` behavior synchronized; verify operator Status still starts/stops with the Status section only

## Workflow follow-up

- Archive after review when implementation is complete.
