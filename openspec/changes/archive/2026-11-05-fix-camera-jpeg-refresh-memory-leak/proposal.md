# Proposal

## Why

A console with many JPEG-refresh camera cells (sample: `камеры` / `kamery-new.json`, 12 cells, 3s refresh) crashes the Chrome tab after roughly 30 minutes. The dual-buffer JPEG path keeps assigning unique cache-busted snapshot URLs to `<img>` elements without releasing prior decoded frames, so browser image/decoder memory grows until the tab OOMs.

## What Changes

- JPEG-refresh camera cells MUST keep long-running memory use bounded: prior snapshot resources MUST be released after each successful swap (and on teardown), so a multi-camera console can stay open for hours without crashing the browser tab.
- Keep the no-flicker dual-buffer behavior (last good frame stays visible until the next load succeeds).
- Skip starting a new JPEG load while a previous refresh for that cell is still in flight.
- Apply the same fix on Place (`data/index.html`) and the simple-user page (`data/user.html`).
- Live mode and direct-to-camera URL rules stay unchanged (no ESP media proxy for console display).

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: Camera-cell JPEG refresh MUST remain stable for long-lived multi-camera consoles (bounded browser memory; no tab crash from unbounded image retention).

## Impact

- Console UI: `data/index.html` and `data/user.html` JPEG refresh helpers (`consoleCameraSwapJpeg` / `consoleCameraArmJpegRefresh` / `buildConsoleCameraView`).
- Specs: `openspec/specs/web-console/spec.md`.
- Firmware version build bump to `4.0.30` (propose).
- No host API, camera registry, ONVIF, or ESP media-proxy changes.
- Reproducing config: `/home/dsporynkhin/Downloads/kamery-new.json` (12 `jpegRefresh` cells).
