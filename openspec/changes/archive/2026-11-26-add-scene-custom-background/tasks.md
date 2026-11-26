# Tasks

## 1. Scene store and SD paths

- [x] 1.1 Add `SCENE_CUSTOM_DIR` (and any path helpers) for SD `scene-custom/<id>.<ext>`; keep Plan bakes on LittleFS — verify Defines/SceneStore compile with both path families
- [x] 1.2 Accept kind `custom` in SceneStore parse/serialize; default missing kind to `plan`; `hasBake` for `custom` checks SD (mount-on-demand) — verify list/item JSON shows kind and hasBake for a custom scene with/without file
- [x] 1.3 Implement custom image write/read/delete on SD (≤1 MiB, PNG/JPEG magic check, clear error without SD); wire wipe on scene delete and full-restore scene wipe — verify reject oversize/bad format/no-SD and successful write appears under SD custom dir

## 2. HTTP bake/background routes

- [x] 2.1 Branch GET `/api/scenes/bake` so `custom` serves from SD with matching Content-Type; Plan unchanged from LittleFS — verify GET returns image for custom and 404 when missing
- [x] 2.2 Branch POST `/api/scenes/bake` (or shared upload path) for `custom` to SD with editScenes auth; Plan JPEG path unchanged — verify upload then GET round-trip on P4 with SD

## 3. Full backup / restore

- [x] 3.1 Pack custom SD images into Full backup ZIP members; keep Plan LittleFS bakes as today — verify ZIP contains custom member when a custom scene has an image
- [x] 3.2 On Full restore wipe/apply, remove and restore SD custom images (fail clearly if SD unusable for those members) — verify restore brings back the custom image after reboot when SD is present
- [x] 3.3 Confirm ordinary Export settings still omits image bytes but includes kind/bakePath/hasBake — verify exported JSON has no embedded image payload

## 4. Scene editor UI

- [x] 4.1 Add **Custom background** to `#sceneKindSelect`; ~2× kind select width in CSS; extend `normalizeSceneKind` / chrome sync — verify option visible and select is roughly twice prior width
- [x] 4.2 Add **Upload image** (visible only for `custom`); hide Edit scene / camera / zones for `custom`; keep pins like Plan; empty copy for no image — verify Upload shown only for custom and Edit scene hidden
- [x] 4.3 Wire upload (client ≤1 MiB check, accept png/jpeg) to bake POST after scene id exists (save-first if needed); refresh bind preview — verify preview shows uploaded image after successful upload

## 5. Live view (Place + simple-user)

- [x] 5.1 Treat `custom` like `plan` in `scene.js` (pins, bake URL, not camera refresh/zones); keep operator and simple-user synchronized — verify Place and user.html both show custom background + pins

## 6. Integration check

- [x] 6.1 Smoke: create Custom background scene, upload ≤1 MiB JPEG, pin a device, Full backup/restore round-trip on P4 with SD; confirm no-SD upload error — verify end-to-end against specs

## Workflow follow-up

- Archive the change after implementation review.
- Host build bumped to **4.6.46** during apply.
