# Design

## Context

Binary sensor bindings already support `viewStyle: icon` + MDI `icon`, rendered via `buildConsoleIconElement` (display-only). On/Off and automation bindings always render as `.console-switch` buttons in Place (`placeBuildDeviceRow`), user page, and Consoles edit preview. The binding dialog only sets `allowsIconStyle` for binary types; automation saves omit `viewStyle`/`icon` entirely. Host store already accepts those fields on bindings. See proposal.md for motivation.

## Goals / Non-Goals

**Goals:**
- Reuse the same View style + icon picker UX for On/Off and automation bindings.
- Make icon controls interactive on Place and user pages (same command / active-toggle paths as switches).
- Keep editor preview display-only.
- Preserve Default = switch behavior and existing binary icon path.

**Non-Goals:**
- Changing widget header group On/Off (stays switch).
- New icon catalog or CDN; keep existing MDI / `BINARY_SENSOR_ICON_CHOICES` (or rename if shared).
- Gauge for On/Off or automation.
- Host schema / API version bumps beyond existing binding fields.

## Decisions

1. **Eligibility in the dialog**  
   Treat `allowsIconStyle` as true when: binary sensor type, **or** device type `onOff` (status/empty param path that already shows a switch), **or** binding target is automation.  
   `fillConsoleDeviceViewStyleOptions` already takes a boolean that means “icon options vs gauge options”; keep that meaning (true → Default/Icon).  
   **Alternative considered:** separate View style enums per target type — rejected; reuse existing `default`/`icon`.

2. **Persist fields for automations**  
   Extend the automation branch of `readConsoleDeviceDialogBody` to include `viewStyle` and `icon` when Icon is selected (same normalization as devices). Missing fields remain Default/switch.  
   **Alternative:** store only on device bindings — rejected; automations need the same persistence.

3. **Interactive icon control**  
   Prefer a button (or clickable element with `role="button"`) that reuses `.console-binary-icon` + `is-on`/`is-off` colors, wires the same onclick as today’s switch (`placeSendCommand` ON/OFF, `setPlaceAutomationActive`, user-page equivalents). Binary sensors stay non-interactive spans.  
   **Alternative:** keep a hidden switch and overlay the icon — rejected as fragile.

4. **Render order**  
   In row builders, check `binding.viewStyle === "icon"` for On/Off and automation **before** creating the switch; fall through to switch when Default or missing. Editor preview mirrors appearance without command handlers.

5. **CSS**  
   Only add affordances needed for pointer/focus on interactive icons (e.g. cursor, keyboard focus ring). Keep active yellow / inactive muted theme already used by binary icons.

## Risks / Trade-offs

- [Automation early-return omits viewStyle today] → Explicit save-path change; verify round-trip after reboot.
- [Icon click vs title hit target] → Keep control in `.console-device-row-controls` only, same as switch.
- [Param not status on On/Off] → Only offer Icon when the dialog would already show the switch control (status/empty param), matching current switch eligibility.

## Migration Plan

No migration script. Existing On/Off and automation bindings without `viewStyle` keep switches. Rollback = revert UI eligibility/render; stored `icon` fields are ignored if View style Default.

## Open Questions

None.
