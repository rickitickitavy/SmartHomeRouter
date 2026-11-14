# Design

## Context

See proposal.md. Console bindings already persist `showTitle` / `titlePosition` and render via `normalizeTitlePosition` / `console-device-row--title-*`. Scene pins currently force untitled in `scenePinToBinding`, `scenePinBindingForPreview`, and `applyBindingBodyToScenePin` (`showTitle: false`, clear `title` / `titlePosition`). Scenes spec previously said “no pin titles.”

## Goals / Non-Goals

**Goals:**
- Optional title + left/top/right/bottom for **gauge** scene pins only.
- Same dialog UX as cell binds when editing a scene gauge pin.
- Persist and render on designer preview, Place, and simple-user scene cells (both pages).

**Non-Goals:**
- Titles on Icon scene pins.
- Changing console cell-bind title defaults.
- Slave or non-UI firmware logic beyond host version bump.

## Decisions

1. **Defaults for scene gauges**  
   Missing `showTitle` → **false** (preserve existing untitled scenes). When enabling title, `titlePosition` default `left` like cells. Title text may default empty or device name at display time if empty but shown — prefer showing stored title or binding fallback consistent with cell binds (`title || device name`).

2. **Dialog**  
   When `consoleDeviceDialogScenePin` and view style is gauge, show the same Show title / Title / Title position controls as non-scene binds (do not hide them for scene pins). Keep Icon scale still hidden for scene pins.

3. **Bridge**  
   Stop clearing title fields in `applyBindingBodyToScenePin` when `viewStyle === "gauge"`. Pass `showTitle` / `titlePosition` / `title` through `scenePinToBinding` and preview binding helpers for gauges. Icon path keeps forcing untitled.

4. **Layout**  
   Reuse existing title-position CSS/classes used by cell gauges inside `.scene-bind-pin` / `.scene-cell-pin`. Fit logic already accounts for `.console-device-row-title` height when present — verify with top/bottom positions on small pins.

5. **Persistence**  
   Pin JSON fields only; SceneStore already serializes pins. Global export/restore already includes scene pins — no separate export schema change beyond field presence.

## Risks / Trade-offs

- **[Risk] Small pins + long titles overflow** → Mitigation: same clipping/wrap as cell gauges; operators size pin outline.
- **[Risk] Accidental titles on icons** → Mitigation: dialog hides controls; apply path clears title fields for non-gauge.
- **[Risk] Operator vs user page drift** → Mitigation: shared helpers / mirror changes in `index.html` and `user.html` scene render paths.

## Migration Plan

- Existing scenes without title fields remain untitled gauges.
- Host UI **4.2.67**; slave unchanged.

## Open Questions

None blocking. Scope limited to gauge pins as requested.
