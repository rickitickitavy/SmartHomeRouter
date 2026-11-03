# Proposal

## Why

JPEG-refresh camera cells blank out on every periodic reload because the console assigns a new `img.src` before the next snapshot finishes loading. Operators see a flicker every few seconds on Place and the simple-user page.

## What Changes

- JPEG-refresh camera cells MUST keep showing the last successfully loaded frame while the next snapshot loads.
- Swap (or assign) to the new image only after that load succeeds; a failed load MUST leave the previous frame visible.
- Apply the same behavior on Place (`data/index.html`) and the simple-user page (`data/user.html`).
- Live mode and direct-to-camera URL rules stay unchanged (no ESP media proxy for display).

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: Camera cell JPEG refresh MUST update without flicker (keep last good frame until the next snapshot loads).

## Impact

- Console UI: `data/index.html` and `data/user.html` camera cell render/refresh path (`buildConsoleCameraView` / JPEG interval).
- Specs: `openspec/specs/web-console/spec.md`.
- Firmware version build bump to `3.10.07` (propose).
- No host API, camera registry, or ONVIF resolve changes.
