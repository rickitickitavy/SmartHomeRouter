# Tasks

## 1. Scene-pin digit and title scaling

- [x] 1.1 Add scene-pin digit CSS (`24em`) and title cqmin clamp in `data/css/all.css`; keep cell base `10.88px` / Option B untouched — verify selectors only target `.scene-cell-pin` / `.scene-bind-pin`
- [x] 1.2 Add `fitScenePinArcDigitFontForSvg` / `fitScenePinArcDigitFonts` (SVG root font-size = width/104 + ResizeObserver) in `data/index.html` and `data/user.html`; wire builder rAF + composite scene-pin path — verify helpers are byte-identical across pages
- [x] 1.3 Hook SceneKit `afterPinsPainted` / overlay after-layout in `data/js/scene.js` and Place/user mount paths — verify layout/resize refits scene digits

## 2. Embed and version

- [x] 2.1 Regenerate embedded web assets and bump `FIRMWARE_VERSION_HOST` build by 1; leave `FIRMWARE_VERSION_SLAVE` unchanged — verify Defines host build and embed header regenerate

## 3. Verification

- [x] 3.1 Confirm `applyCompositeContentFit` Option B constants and non-scene `.console-gauge-value-svg` `10.88px` still match pre-change cell behavior (no cell retune)
- [ ] 3.2 After host rebuild/flash: confirm scene pin digits match `21-31-18` and labels near `21-32-10` across large/small windows on Place and simple-user; cell gauges (e.g. Отопление) unchanged

## Workflow follow-up

- Archive after review when verification passes.
