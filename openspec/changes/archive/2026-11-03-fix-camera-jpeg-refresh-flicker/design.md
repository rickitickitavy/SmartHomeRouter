# Design

## Context

See proposal.md — Why. Today `buildConsoleCameraView` (identical on Place and simple-user) sets `img.src` to a cache-busted snapshot URL on a 3s interval. Assigning `src` clears the painted frame until the next decode completes, which reads as flicker. Display still MUST use the camera’s HTTP snapshot URL directly (no ESP media proxy).

## Goals / Non-Goals

**Goals:**

- Eliminate blanking between successful JPEG refreshes on both console surfaces.
- Keep the previous good frame if a refresh fails or is slow.
- Preserve interval, cache-busting, live mode, and direct-URL rules.

**Non-Goals:**

- Changing refresh interval, camera registry APIs, or ONVIF resolve.
- Proxying or buffering JPEG bytes on the ESP for console display.
- Changing Live mode playback.

## Decisions

1. **Dual-buffer JPEG stack**  
   Keep two stacked `<img>`s (front/back). Load the next snapshot into the hidden back image; on `onload` swap front/back classes. Never change `src` on the currently visible front image. Failed loads leave the front frame alone.  
   *Alternatives considered:* preload into off-DOM `Image` then assign visible `src` (still flashes in browsers); blob fetch (CORS risk).

2. **Reuse live camera hosts across console re-renders**  
   Place and simple-user rebuild the whole console on device upserts. Cache camera view hosts by console/widget/cell/binding and re-append them so telemetry updates do not remount every camera at once. Restart staggered JPEG timers when a host is reattached.

3. **Same helpers on both pages**  
   Update `data/index.html`, `data/user.html`, and shared `data/css/all.css` in lockstep.

4. **First paint**  
   Initial front image loads directly; only later refreshes use the back buffer.

## Risks / Trade-offs

- [Slow camera] → Old frame stays longer; acceptable vs blank flicker.  
- [Overlapping in-flight loads] → Ignore stale `onload` if a newer request was started, or only apply if `img` still connected and optionally track a generation counter.  
- [Memory] → Off-DOM `Image` is short-lived; revoke not needed for URL strings.

## Migration Plan

- Ship in firmware `3.10.07` web assets via normal OTA. No settings migration. Rollback = prior firmware build.
