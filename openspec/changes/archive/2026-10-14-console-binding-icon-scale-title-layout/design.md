# Design

## Context

Bindings already store `viewStyle` / `icon` and render MDI icons (binary display-only; On/Off and automation interactive). Rows always show a left title and right controls. Widget viewports (vertical / horizontal / grid) fit controls into slots via CSS/layout. See proposal.md for motivation.

## Goals / Non-Goals

**Goals:**
- Persist and apply `iconScale` (default 1) only as a **multiplier** on viewport-fit icon size.
- Persist and apply `showTitle` / `titlePosition` for all non-spacer bindings.
- Dialog fields for all targets; Place and user pages match.

**Non-Goals:**
- Widget-level title layout (widget header unchanged).
- Scaling switches, gauges, or cover buttons via `iconScale` (icon View style only).
- Changing viewport fit algorithm itself beyond composing with `iconScale`.

## Decisions

1. **Field names and defaults**  
   - `iconScale`: float, default `1`; clamp `<= 0` or NaN → `1` on load/save.  
   - `showTitle`: bool, default `true`.  
   - `titlePosition`: `left` | `right` | `top` | `bottom`, default `left`.  
   **Alternative:** store scale as percent integer — rejected; float matches CSS scale.

2. **Compose with viewport fit**  
   Apply viewport/cell fit first (existing layout), then set icon visual size with `transform: scale(iconScale)` or `font-size: calc(base * iconScale)` **after** fit, so fit scale × iconScale. Prefer a CSS custom property `--console-icon-scale` on the icon element so viewport rules stay untouched.  
   **Alternative:** bake a single inline font-size from JS measuring the cell — rejected as fragile vs multiply-after-fit.

3. **Title layout**  
   Drive row layout with modifier classes, e.g. `console-device-row--title-left|right|top|bottom` and `console-device-row--no-title`, using flex direction/order so controls and title swap without duplicating markup. Omit the title node when `showTitle` is false.  
   **Alternative:** absolute positioning — rejected; flex keeps a11y and wrapping simpler.

4. **Dialog UX**  
   Always show Show title (+ Title position when checked) for device and automation targets. Show Icon scale only when View style is Icon (same group as the icon picker).  
   **Alternative:** nest title options under a “Layout” accordion — unnecessary for three fields.

5. **Host store**  
   Extend `ConsoleBinding` with `float iconScale`, `bool showTitle`, `uint8_t titlePosition`; parse/serialize JSON; omit defaults from JSON when equal to defaults if that matches existing style, or always write — follow neighboring fields’ pattern (`viewStyle` only written when non-default).

## Risks / Trade-offs

- [Large `iconScale` overflows grid cell] → Accept overflow; operators choose scale; document multiply semantics.
- [Title top/bottom in horizontal viewport] → Flex column inside the cell; may reduce control height — acceptable.
- [Stacked with unarchived icon View style change] → Implement on current branch that already has icon On/Off/automation support.

## Migration Plan

Missing fields → defaults (`iconScale` 1, `showTitle` true, `titlePosition` left). No migration script. Rollback ignores unknown JSON keys on older firmware.

## Open Questions

None — assumed Icon scale only when View style is Icon; title fields for all bindings including gauges and switches.
