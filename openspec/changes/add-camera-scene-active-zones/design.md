# Design

## Context

- Plan scenes: bake JPEG + pins (device/automation, Icon/Gauge).
- Consoles already refresh camera JPEG every `CONSOLE_CAMERA_REFRESH_MS` (3000) and support per-binding click actions for current ON/OFF state.
- User vision: Camera scene kind with live camera background and transparent active polygons instead of pins.

## Goals / Non-Goals

**Goals**

- Scene `kind`: `plan` | `camera` (missing kind ⇒ `plan`).
- Camera background from selected registered camera; refresh every scene `refreshInterval` seconds (per-scene; default **3**; reasonable clamp e.g. **1..60**).
- Active zones: polygons; edit like floor polygons **without Shift**; runtime transparency; ON/OFF colors when not fully transparent; design-time always **70%** transparent.
- State from bound device/reportable; click runs action for **current** state (console-equivalent: device or automation action per state).
- Camera scenes: **zones only** (no pins). Plan scenes: pins as today.
- Kind dropdown before Edit scene; camera dropdown after Edit scene when kind is Camera.
- Zones/normalized geometry scale with the displayed camera image box.
- Persist + export/restore.

**Non-Goals**

- Live/WebRTC camera mode for scenes (JPEG refresh only unless already shared helpers make live trivial — stick to JPEG refresh).
- Changing Plan geometry tools (walls/windows/doors) for Camera kind.
- Slave / SPI / OTA.

## Decisions

1. **Kind field** — `scene.kind` / model: `plan` | `camera`. Legacy ⇒ `plan`.
2. **Camera scenes store** — `cameraId`, `refreshInterval` (seconds), `zones[]`; no pins required/used in UI for Camera.
3. **Zone model** — polygon points in 0..1 image-box space; `transparency` 0..1 (1 = fully transparent); `colorOn` / `colorOff` required when transparency &lt; 1; binding for state; `actionOn` / `actionOff` (same shape as console click actions / scene-pin-compatible device|automation actions).
4. **Fully transparent** — still clickable; no ON/OFF fill colors required; design-time still 70% transparent.
5. **Click** — resolve current binary state from binding; run that state’s action (like `makeBindingClickActionHandler(binding, isOn)`).
6. **Edit UX** — Camera “Edit scene” opens zone editor over camera still (or last frame), polygon tools analogous to floor (add/move vertices) **without** Shift axis lock; camera picker fixed dropdown after Edit scene when kind is Camera.
7. **Refresh** — default 3 s (match console JPEG); per-scene field; clamp 1..60 s.
8. **Both pages** — Place and simple-user render Camera scenes the same way.

## Risks / Trade-offs

- Heavy JPEG refresh for many Camera scene cells: reuse console camera refresh staggering if practical.
- Missing/deleted camera: show empty/unavailable state without crashing editor or Place.

## Open Questions

- None material; defaults above apply unless apply discovers a tighter product constraint.
