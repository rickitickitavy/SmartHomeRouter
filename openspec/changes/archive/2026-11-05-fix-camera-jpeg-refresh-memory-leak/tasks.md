# Tasks

## 1. JPEG refresh resource lifecycle (Place)

- [ ] 1.1 In `data/index.html`, track per-host blob URLs for front/back (and in-flight) and add a teardown helper that clears the JPEG timer, disarms refresh, revokes held object URLs, clears both `img.src`, and nulls img refs — verify by reading that `showUnavailable` and host-replace paths call it
- [ ] 1.2 Rewrite `consoleCameraSwapJpeg` on Place to prefer `fetch` → `createObjectURL` → back `img`, swap only on `onload`, revoke the previous front blob after swap, and gate with `_cameraJpegLoading` so a cell never stacks in-flight loads — verify generation mismatch / error paths revoke without swapping and leave the front frame visible
- [ ] 1.3 Point initial JPEG paint on Place through the same blob (or HTTP clear-src fallback) path so the first frame is also revocable — verify first paint still shows a snapshot without flicker on the next refresh

## 2. Sync simple-user page

- [ ] 2.1 Mirror the same blob/teardown/in-flight changes in `data/user.html` camera helpers so Place and simple-user stay identical — verify a side-by-side diff of the camera JPEG helpers shows no behavioral drift
- [ ] 2.2 Bump `FIRMWARE_VERSION` build by 1 in `include/Defines.h` as part of the source apply (even if propose already bumped) — verify the string matches the apply increment

## 3. Verification

- [ ] 3.1 Open the `камеры` console (config like `kamery-new.json`, ~12 JPEG cells) on Place and confirm refreshes keep the last good frame with no blank flashes — verify visually for several refresh cycles
- [ ] 3.2 Leave that multi-camera console open and watch Chrome Task Manager / Memory for the tab: image/JS memory MUST NOT climb without bound over a long soak (target: stable after warm-up, no tab crash by 30+ minutes) — verify memory plateaus rather than ramping
- [ ] 3.3 Repeat a short refresh/no-flicker check on the simple-user page for the same console — verify both surfaces behave the same

## Workflow follow-up

- Archive the change after review and soak verification are satisfied.
