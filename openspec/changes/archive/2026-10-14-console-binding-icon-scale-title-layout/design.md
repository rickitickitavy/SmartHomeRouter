# Design

## Context

Bindings already store `viewStyle` / `icon` and render MDI icons (binary display-only; On/Off and automation interactive). Rows always show a left title and right controls. Widget viewports (vertical / horizontal / grid) fit controls into slots via CSS/layout. See proposal.md for motivation.

## Goals / Non-Goals

**Goals:**
- Persist and apply `iconScale` (default 1) only as a **multiplier** on viewport-fit icon size.
- Persist and apply `showTitle` / `titlePosition` for all non-spacer bindings.
- Persist and apply `alignH` / `alignV` (default `center`) for all non-spacer bindings in every View style.
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
   - `alignH`: `left` | `center` | `right`, default `center`.  
   - `alignV`: `top` | `center` | `bottom`, default `center`.  
   - Legacy `iconAlign` on load → `alignH` when `alignH` is missing; still written for icon bindings when non-default for older clients.  
   **Alternative:** store scale as percent integer — rejected; float matches CSS scale.

2. **Compose with viewport fit**  
   Apply viewport/cell fit first (existing layout), then set icon visual size with `transform: scale(iconScale)` or `font-size: calc(base * iconScale)` **after** fit, so fit scale × iconScale. Prefer a CSS custom property `--console-icon-scale` on the icon element so viewport rules stay untouched.  
   **Alternative:** bake a single inline font-size from JS measuring the cell — rejected as fragile vs multiply-after-fit.

3. **Title layout**  
   Drive row layout with modifier classes, e.g. `console-device-row--title-left|right|top|bottom` and `console-device-row--no-title`, using flex direction/order so controls and title swap without duplicating markup. Omit the title node when `showTitle` is false.  
   **Alternative:** absolute positioning — rejected; flex keeps a11y and wrapping simpler.

4. **Dialog UX**  
   Always show Show title (+ Title position when checked), Horizontal align, and Vertical align for device and automation targets (all View styles). Show Icon scale only when View style is Icon (same group as the icon picker). Remove the Icon-only “Icon align” control in favor of the shared Horizontal align field.  
   **Alternative:** nest title options under a “Layout” accordion — unnecessary for these fields.

5. **Host store**  
   Extend `ConsoleBinding` with `float iconScale`, `bool showTitle`, `uint8_t titlePosition`, `uint8_t alignH`, `uint8_t alignV`; parse/serialize JSON; omit defaults from JSON when equal to defaults if that matches existing style — follow neighboring fields’ pattern (`viewStyle` only written when non-default).

6. **Content align CSS**  
   Apply `console-device-row--align-h-*` / `--align-v-*` for every non-spacer binding so Default/Gauge/Icon honor the same cell alignment in grid and other viewports.

## Risks / Trade-offs

- [Large `iconScale` overflows grid cell] → Accept overflow; operators choose scale; document multiply semantics.
- [Title top/bottom in horizontal viewport] → Flex column inside the cell; may reduce control height — acceptable.
- [Stacked with unarchived icon View style change] → Implement on current branch that already has icon On/Off/automation support.
- [Title position + content align both set flex properties] → Prefer align modifiers with sufficient specificity in grid cells; accept minor differences in vertical/horizontal viewports.

## Migration Plan

Missing fields → defaults (`iconScale` 1, `showTitle` true, `titlePosition` left, `alignH`/`alignV` center). If only legacy `iconAlign` is present, use it as `alignH`. No migration script. Rollback ignores unknown JSON keys on older firmware.

## Open Questions

None — assumed Icon scale only when View style is Icon; title and content-align fields for all bindings including gauges and switches.
