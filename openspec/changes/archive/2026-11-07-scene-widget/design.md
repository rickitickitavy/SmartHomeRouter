# Design

## Context

Consoles already support composite cells with `empty` / `text` / `device` / `camera`. Users have roles including `editConsoles`. See proposal.md for why scenes become entities + cell content instead of a widget viewport. Designer geometry/bake/pin rules from the earlier scene-widget draft remain the product intent; only the hosting surface changes (Scenes tab + cell reference).

## Goals / Non-Goals

**Goals:**

- First-class Scene entities with designer + Full HD bake + relative pin overlays.
- Role `editScenes` gates Scenes tab management; console viewers need only console assignment.
- Composite cell content `scene` for mixing with cameras/devices/text.
- Keep fullscreen (session-only) and history smooth fixes (no EMA; Smooth off; samples 5).
- Lightweight custom Canvas renderer (no three.js).

**Non-Goals:**

- Scene as a widget `viewport`.
- Furniture/prop object types.
- Live WebGL on console view.
- Letting simple users without `editScenes` manage scenes.
- Persisting full-screen preference.

## Decisions

### 1. Scene entity + cell reference

- **Choice:** Persist scenes outside console JSON (own store/API). Composite cells store `content: "scene"` and `sceneId`. Pins (device/automation, relative x/y, icon/gauge) live on the scene.
- **Why:** One plan reused in many cells; wide screens can mix scenes and cameras.
- **Alternatives:** Scene widget viewport — rejected; duplicate pins per console — rejected.

### 2. Role `editScenes`

- **Choice:** New boolean on user records. Scenes tab and scene mutate APIs require `editScenes` or `isAdmin`. `isAdmin` implies the right even if the flag is false. Viewing scene cells on Place/`user.html` uses existing console assignment only.
- **Why:** Matches option A with an explicit role (not bundled into `editConsoles`).
- **Alternatives:** Fold into `editConsoles` — weaker separation.

### 3. Designer host

- **Choice:** Scenes tab on the gateway operator console (visible when `editScenes`/`isAdmin`). Same tools as previously specified: top-view edit, View scene toggle, orbit about center, join, bake ≥1920×1080 JPEG on Save, Cancel discards. Normalized sceneSize default `1.0`, wall height default `0.15`, wall length `sceneSize/7`, multi-floor with floor height, no props in v1.
- **Why:** Authoring is admin work; bake is what consoles display.

### 4. Bake storage

- **Choice:** Scene model JSON in scene store; bake as LittleFS JPEG under `/scene-bakes/` referenced by path; export/import rehydrate bake.
- **Why:** Avoid huge base64 in main consoles.json.

### 5. Console rendering

- **Choice:** Scene cell shows bake (aspect-preserved) + overlays at relative coords; Icon/Gauge per existing eligibility; no titles on pins; clicks match existing icon/gauge controls. Same on Place and `user.html`.
- **Why:** Spec parity with earlier binding behavior, new host surface.

### 6. Rollback of viewport approach

- **Choice:** Remove `viewport: scene` / scene-widget editor chrome from consoles; keep history smooth + fullscreen from this change’s apply work.
- **Why:** Product direction changed mid-apply.

### 7. Fullscreen + history (unchanged intent)

- Theme-row full-screen toggle, session-only, both account tabs.
- History: average-only smooth; default Smooth off; samples 5; both pages.

## Risks / Trade-offs

- **[Risk] Orphan scene ids in cells after scene delete** → Mitigate: on delete, clear or reject cells referencing that id; show empty placeholder in view.
- **[Risk] Bake size × many scenes** → Mitigate: JPEG quality cap; delete bake file with scene.
- **[Risk] Role sprawl** → Mitigate: single checkbox next to Console editor; document in Security UI.

## Migration Plan

- No prior production `viewport: scene` to migrate (draft only).
- Missing `editScenes` loads as false; `isAdmin` still manages scenes.
- Missing cell content `scene` ignored by older firmware as unknown content → treat as empty on load if needed.

## Open Questions

- Exact scene id format and max scene count (decide at implement; e.g. mirror camera/console id style).
- Whether Scenes appears in sidebar next to Cameras or Consoles (prefer after Cameras / before Consoles unless UI convention says otherwise).
