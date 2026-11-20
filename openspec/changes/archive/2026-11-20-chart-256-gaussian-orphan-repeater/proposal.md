## Why

History charts currently fetch at most 128 points, which is coarse for longer ranges. Chart smoothing is a flat average with default window 5, while operators want a Gaussian-weighted window with a clearer default. Separately, when a Repeater is unpaired, registered devices can keep a stale `routerId`; the device dialog’s ROUTER field stays read-only, so operators cannot reassign those orphans without deleting the device.

## What Changes

1. Raise history chart / `/api/history` `maxPoints` ceiling from **128** to **256** (UI request and server clamp/default).
2. Replace average-window chart smooth with a **Gaussian** weighted window: `real_samples_count = user_samples * 2 + 1`; default user samples **3**; Smooth remains unchecked on open.
3. When a registered device’s `routerId` points at a **missing** paired Repeater (orphan), make ROUTER an editable fixed `<select>` of **local (Main)** plus currently paired Repeaters; Save persists the new `routerId` so the device can be reassigned without delete.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `historical-data`: History query API max point bound 256.
- `web-console`: Chart fetch 256; Gaussian smooth + default 3; orphan ROUTER dropdown on device dialog (operator page).
- `router-interconnect`: Operator may reassign a registered device’s `router_id` when the prior Repeater is no longer paired; local = `0`.

## Impact

- Host web UI (`data/index.html`, `data/user.html` for chart), `WebConsole` history handler, device Save payload `routerId`.
- Host firmware build bump only (`FIRMWARE_VERSION_HOST` → **4.5.13**). No slave radio change.
- AsyncTCP history response slightly larger (up to 256 points); keep PSRAM/heap sample buffer pattern.
