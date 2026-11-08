## Context

Commit `34c20fe` removed `#consoleDeviceIconScale` from the shared Binding dialog when scene pins moved to outline `sizeW`/`sizeH`. That also removed Scale for classic console Icon bindings, which still must persist and apply `iconScale` (specs already require it). Export/Import were added on the geometry editor footer; the operator wants them on the bind editor (bake + pins). Pin `x`/`y` are normalized with `frame.getBoundingClientRect()` while `.scene-bind-image` / bake use `object-fit: contain`, so overlays cover letterbox bars and pins drift when the frame aspect changes. Scene pin gauge value CSS uses a small `cqmin` clamp (~8px).

## Goals / Non-Goals

**Goals:**
- Restore Icon scale UI + apply path for classic (and composite) Icon bindings; hide Scale only for scene pin dialogs.
- Move Export / Export scheme / Import to the scene bind editor; remove them from the geometry editor.
- Map pin coordinates and overlay layout to the bake image content box in bind editor and Place/user scene cells.
- ~2× scene pin gauge value text size.

**Non-Goals:**
- Changing scene pin outline resize (`sizeW`/`sizeH`) model.
- Reintroducing Scale on scene pin dialogs.
- Slave firmware changes.

## Decisions

1. **Icon scale restore** — Re-add the Icon scale number field next to Icon config; show when View style is Icon **and** `!consoleDeviceDialogScenePin`. Read/write `binding.iconScale` as before. Keep `delete pin.iconScale` for scene pin save. Ensure composite/grid layout paths still multiply fit × `data-binding-scale` / `iconScale`.

2. **Export/Import placement** — Wire the same full/scheme/import helpers to buttons on `#sceneEditDialog` (bind editor) actions. Drop those buttons from `#sceneGeometryDialog`. Import still targets the in-memory scene model / draft used by the bind editor (empty-check unchanged in spirit; may import model then refresh bake preview as designed).

3. **Image-relative pins** — Helper: given frame + `img`, compute content rect from naturalWidth/Height and contain fit (or `getClientRects` / computed object-fit box). Normalize pointer → 0..1 against that rect; position overlays with matching left/top/width/height on the content box (not `inset:0` on the full frame). Share helper for bind editor and `paintSceneCellPins` / `mountSceneCell` paths so Place and user.html stay in sync.

4. **Gauge text** — Raise scene-pin / scene-cell gauge value font (`clamp` / SVG text) to ~2× current (e.g. ~8cqmin → ~16cqmin, adjust SVG `font-size` the same way). Keep readable within the pin box.

## Risks / Trade-offs

- Existing pins authored against full-frame coords may shift once mapping becomes image-relative; that is the intended fix for wrong placement. No migration of stored numbers unless a one-shot remap is later requested.
- Import on bind editor may need a post-import bake refresh if model changes without regenerating JPEG — note in tasks; prefer open geometry Save path or clear pending bake as existing UX allows.

## Migration Plan

- No server schema change. Re-flash host web assets with bumped `FIRMWARE_VERSION_HOST`.

## Open Questions

None blocking. Assumption: “empty” import gate still applies to the scene model being imported into from the bind editor.
