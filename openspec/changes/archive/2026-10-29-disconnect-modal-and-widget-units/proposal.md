# Proposal

## Why

When the browser loses the router (HTTP or live WebSocket), the signed-in UI keeps looking “live” with no clear signal that data and controls may be stale. Separately, console widget binding units are chosen from the device’s classified type, so a multi-value device (reportable temperature, humidity, battery, and similar) can force Celsius (or another type-wide unit) onto every selected property. Linear gauges today only grow a single fill bar horizontally; operators need arc-like filled bands and vertical layouts in one clear control.

## What Changes

- Add a blocking modal on signed-in operator and simple-user pages when the main HTTP session/reachability link or a required live WebSocket drops: title **Disconnected**, with an appropriate disconnected picture/icon.
- While the UI is actively trying to restore that link, keep the same modal and switch the picture/icon to a **RECONNECTING** state with text that the UI is reconnecting to the router.
- Hide the modal when connectivity is restored. Do not treat intentional sign-out or the existing firmware-reboot wait overlay as this disconnect flow.
- When adding or editing a device binding on a console widget, measurement unit options (and when the unit field is shown) MUST follow the **selected binding parameter**, not the device’s global type alone—so temperature, humidity, battery, and other properties each get appropriate units (e.g. °C/°F/K vs % vs no temperature unit).
- For linear gauges, add **one** persisted style option with **four** states (not two independent flags):
  - `bar` — current horizontal growing fill (default when missing)
  - `filled` — horizontal; full range-colored band like the arc, straightened, with a small value arrow
  - `vertical` — vertical growing fill; uses maximum available container height after the binding label
  - `verticalFilled` — vertical filled band + small arrow; same max-height rule
- Arc gauges ignore this option. Editor shows the four-state control only when gauge type is Linear.

## Capabilities

### New Capabilities

- `console-connection-overlay`: Signed-in browser UI feedback when the Mini-HA router HTTP or live WebSocket link is down or reconnecting.

### Modified Capabilities

- `control-consoles`: Device-binding measurement units from selected parameter; linear gauge four-state style (bar / filled / vertical / verticalFilled) with vertical max-height vs label.
- `web-console`: Binding dialog unit refresh from selected param; linear style select with the four states when type is Linear.

## Impact

- Frontend: `data/index.html`, `data/user.html`, `data/css/all.css` — connection overlay; unit resolver; linear gauge render/layout; binding editor select.
- Host persist: `ConsoleGaugeConfig` / `ConsoleStore` JSON round-trip for the new linear style field (e.g. `linearStyle`).
- Firmware build bumped to `3.9.10` for this propose. No new HTTP API routes.
