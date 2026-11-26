# Proposal

## Why

Operators need to control devices on a live camera picture. Today scenes only use a baked plan image with icon/gauge pins. Camera scenes need a refreshing camera background and clickable transparent active zones instead of pins.

## What Changes

- Add scene kind **Plan** | **Camera** (existing scenes are Plan).
- **Camera** scenes: background is the picked camera’s JPEG, refreshed every scene `refreshInterval` seconds; **active zones** (polygons) instead of pins; no icon/gauge pins on Camera scenes.
- **Plan** scenes: keep bake + pins (unchanged authoring model aside from the kind selector).
- Active zones: polygon edit like floor plan authoring **without Shift**; runtime fill uses zone transparency; if not fully transparent, ON and OFF colors; design-time fill is always **70%** transparent for visibility.
- Zone state (ON/OFF color) comes from a **bound device/reportable value**; click runs the action for the **current** state (same pattern as console click actions — device or automation).
- Zones scale with the scene (normalized to the visible camera image box, like plan pin math on the bake).
- Design UI: fixed dropdown **Camera** / **Plan** before **Edit scene**; **Edit scene** only for Plan (hidden for Camera); when kind is Camera, a fixed dropdown of registered cameras after kind.
- Persist kind, camera id, refresh interval, and zones with the scene; include in scene export and global settings export/restore.

## Capabilities

### New Capabilities
- *(none)*

### Modified Capabilities
- `scenes`: Plan vs Camera kinds; camera background; active zones; kind/camera UI.
- `web-console`: scene editor chrome (kind + camera selectors); Place / simple-user Camera scene rendering and clicks.

## Impact

- Host web UI (`data/index.html`, `data/user.html`, scene JS/CSS as needed), scene store JSON shape; host version **4.6.21**.
- Reuses existing camera JPEG refresh path (console default interval **3 s** as scene default).
- No slave changes; no fragile SPI/OTA zones.
