# Design

## Context

See proposal.md — Why. Sample console `kamery-new.json` (`камеры`) has one composite widget with **12** `jpegRefresh` cells. Helpers in `data/index.html` and `data/user.html` are identical:

- Dual-buffer stack (`_cameraFrontImg` / `_cameraBackImg`) from `fix-camera-jpeg-refresh-flicker`.
- Every `CONSOLE_CAMERA_REFRESH_MS` (3000) call to `consoleCameraSwapJpeg` assigns `back.src = url + "?_=" + Date.now()` (unique URL each tick).
- No `revokeObjectURL`, no clear of the retiring front `src`, and no “one in-flight load” gate.

Observed failure mode matches Chrome tab OOM after ~30 minutes: unbounded decoded-image / HTTP-cache retention from unique snapshot URLs × 12 cameras × ~20 refreshes/min.

Display MUST still use the camera’s HTTP snapshot URL directly (no ESP media proxy). Last-good-frame / no-flicker MUST stay.

## Goals / Non-Goals

**Goals:**

- Bound per-cell browser memory for long-lived JPEG refresh on Place and simple-user.
- Keep dual-buffer no-flicker and staggered timers.
- Stop stacking overlapping loads when a camera is slow.

**Non-Goals:**

- Changing refresh interval, camera registry, ONVIF, or Live mode.
- Proxying JPEG bytes through the ESP for console display.
- Fixing unrelated polling/WebSocket memory on the console pages.

## Decisions

1. **Fetch snapshot as blob → `URL.createObjectURL` → assign to back `<img>` → revoke after swap**  
   On each refresh, `fetch(snapshotUrl, { cache: "no-store", mode: "cors" })` (or `no-cors` only if we must fall back — prefer CORS so the response is readable). Build an object URL, set `back.src` to that blob URL. On successful `onload` and front/back swap, `URL.revokeObjectURL` the **previous** front blob URL (and any superseded in-flight blob). On `onerror` / generation mismatch, revoke the failed blob without swapping.  
   *Why:* Object URLs are explicitly revocable; unique `?_=` query strings are not, and Chrome tends to retain decoded frames for distinct HTTP URLs.  
   *Alternatives considered:* Keep HTTP URLs but set retiring `img.src = ""` / `about:blank` (helps some builds, still fights HTTP cache); drop cache-buster (stale frames / CDN cache); canvas `drawImage` + clear (heavier, more code).

2. **One in-flight refresh per host**  
   If `_cameraJpegLoading` is true, skip `consoleCameraSwapJpeg` body (timer still reschedules). Clear the flag on load/error/abort. Prevents N overlapping decodes when a camera is slower than 3s.

3. **Teardown / unavailable path**  
   `showUnavailable` and host replace MUST: `clearTimeout(_cameraJpegTimer)`, set `_cameraJpegArmed = false`, revoke any tracked blob URLs on front/back, clear `src` on both imgs, null refs. When `liveCacheKey` replaces a cached host, tear down the previous host the same way.

4. **CORS failure fallback**  
   If `fetch` fails due to CORS (common on some camera firmwares), fall back to assigning the cache-busted HTTP URL to the back img **and** clear the previous front img `src` after swap (`front.src = ""` then leave class swap). Prefer blob path when fetch succeeds. Document that CORS-enabled cameras get the stronger leak fix; HTTP fallback still clears retiring `src`.

5. **Same helpers on both pages**  
   Edit `data/index.html` and `data/user.html` in lockstep (sync-operator-simple-user-ui). No CSS change required unless blob path needs none.

6. **Initial paint**  
   First frame may use the same blob (or HTTP) path as refresh so both buffers only ever hold revocable/cleared resources.

## Risks / Trade-offs

- [Cameras without CORS] → Blob fetch fails; HTTP+clear-src fallback still used. Memory improvement may be smaller but better than today.  
- [Extra fetch vs `<img src>`] → One XHR/fetch per refresh per camera; acceptable vs tab crash.  
- [Auth cookies / Digest on snapshot URL] → `fetch` credentials mode may need `include` if cameras rely on browser auth; keep parity with what `<img>` would send where possible. Prefer same-origin-relative or absolute URLs already stored on the camera.  
- [Flicker regression] → Only swap after `onload`; never clear front before back is ready.

## Migration Plan

- Ship in firmware `4.0.30` web assets via normal OTA. No settings migration. Rollback = prior firmware build.

## Open Questions

- None that block the approach: CORS fallback covers cameras that reject readable fetch.
