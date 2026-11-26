# Design

## Context

Scenes today are `plan` (LittleFS JPEG bake from geometry + pins) or `camera` (live JPEG + zones). Bake files live under LittleFS `/scene-bakes/`; Full backup packs them as `scenes/bakes/<id>.jpg`. SD on P4 is mount-on-demand and unmounted between history saves to free RAM (`sd-card-storage`). `SCENE_KIND_BUF` is 8 (`custom` fits). Scene kind select max-width is 14rem. See proposal.md for motivation.

## Goals / Non-Goals

**Goals:**
- Third kind `custom` with Plan-like pin authoring and an operator-uploaded still background on SD.
- Upload UI gated to that kind; kind select wide enough for “Custom background”.
- Full backup/restore carry custom image bytes; ordinary settings export stays metadata-only.
- Enforce ≤1 MiB and PNG/JPEG (lightweight still) formats.

**Non-Goals:**
- LittleFS fallback when SD is absent.
- Geometry Edit scene / plan bake generation for custom kind.
- Camera-style zones or refresh on custom kind.
- Decoding/resizing images on device beyond store-and-serve.
- Changing history SD unmount policy beyond mount sessions needed for custom image I/O.

## Decisions

1. **Kind id `custom`, label “Custom background”**  
   Fits `SCENE_KIND_BUF`. Missing/unknown kind still defaults to `plan`.  
   *Alt:* `bg` / `image` — less clear in exports.

2. **Reuse `bakePath` / `hasBake` / `/api/scenes/bake` for the custom image**  
   Custom scenes store path in `bakePath`; GET/POST bake routes branch on kind (LittleFS vs SD). UI keeps the same preview URL.  
   *Alt:* new `/api/scenes/background` — more surface for little gain.

3. **SD directory `scene-custom/` (relative to SD mount root)**  
   Path pattern `scene-custom/<id>.<ext>` (`.jpg` / `.png` from content). Kind `custom` implies SD FS; Plan stays on LittleFS `/scene-bakes/`.  
   *Alt:* put custom files under LittleFS — rejected (user required SD).

4. **SD required — no LittleFS fallback**  
   Upload fails with a clear error if SD is missing/unusable (typical S3 / SD-less P4). Kind remains selectable; empty state until a successful upload.

5. **Mount-on-demand for custom image I/O**  
   Mount for upload, GET serve, hasBake existence check when needed, Full backup pack, and Full restore wipe/write; then unmount so idle RAM stays freed like history. Static Place/user load issues one GET (not a refresh loop).  
   *Alt:* keep SD always mounted when any custom scene exists — conflicts with RAM-free unmount policy.

6. **Authoring chrome**  
   - Kind select: options Plan | Camera | Custom background; ~2× width (`max-width` ~28rem on `#sceneKindSelect` or a dedicated class).  
   - **Edit scene**: Plan only.  
   - **Upload image**: Custom only (file input accept `image/png,image/jpeg,.png,.jpg,.jpeg`; client reject >1 MiB before POST).  
   - Camera picker / refresh / zones: Camera only.  
   - Pins: Plan and Custom.  
   - Empty copy for custom: “No image — upload an image”.

7. **Upload validation (host)**  
   Body ≤ 1 MiB; magic-byte check for JPEG (`FF D8`) / PNG (`89 50 4E 47`); reject others with 400. Content-Type on GET matches stored format.

8. **Full backup members**  
   Pack custom images as `scenes/custom/<id>.<ext>` (or keep `scenes/bakes/` with extension) from SD; Plan bakes unchanged from LittleFS. Full restore wipe deletes SD `scene-custom/` members as well as LittleFS bakes before apply. Ordinary Export settings still omits image bytes.

9. **Live view**  
   `scene.js` treats `custom` like `plan` for pin overlays (not camera). Bake URL remains `/api/scenes/bake?id=…`. Sync Place and simple-user.

## Risks / Trade-offs

- [SD remount latency / RAM on each image GET] → Accept for static loads; avoid polling; reuse history mount helpers carefully (mutex if shared with history save).
- [S3 / no SD cannot persist custom backgrounds] → Clear upload error; documented; no silent LittleFS fallback.
- [1 MiB still images on SD vs flash wear] → SD is appropriate; limit enforces lightweight assets.
- [Full backup larger / restore needs SD present] → Restore of custom images fails clearly if SD unavailable; Plan/camera parts may still apply per existing restore rules — prefer fail the custom image members with logged error rather than writing them to LittleFS.

## Migration Plan

- Existing scenes unchanged (no kind → plan).
- No automatic migration of Plan bakes to SD.
- Rollback: remove `custom` option and SD paths; leftover `scene-custom/` files on SD are inert orphans (operator may delete manually).

## Open Questions

- None that block specs/tasks; WebP support can stay out unless a tiny decoder/serve-as-file path is already free (serve-as-file only needs Content-Type — optional follow-up if accept list grows).
