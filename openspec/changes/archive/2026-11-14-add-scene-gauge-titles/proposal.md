# Proposal

## Why

Scene pins currently force titles off (`showTitle` false), including gauge pins, so operators cannot label gauges on floor plans the way they already label bindings in console cells. Gauge pins on scenes need the same optional title and left/top/right/bottom placement as cell binds.

## What Changes

- For **scene pins with view style Gauge** (P4 and S3 host UI): enable optional **Show title**, **Title** text, and **Title position** (`left` / `top` / `right` / `bottom`), matching console cell binding behavior.
- Persist those fields on the scene pin through save, reboot, scene export/import, and global settings export/restore (pins already travel with scenes).
- Render titles on scene designer live preview, Place scene cells, and simple-user scene cells with the chosen position; when Show title is off, no title (current look).
- Icon (and non-gauge) scene pins keep today’s no-title behavior unless already otherwise specified.
- Stop stripping `title` / `showTitle` / `titlePosition` for gauge pins in the scene pin ↔ binding bridge.

## Capabilities

### New Capabilities
- *(none)*

### Modified Capabilities
- `scenes`: gauge pins may carry optional title + title position; render accordingly.
- `web-console`: scene pin binding dialog exposes Show title / Title / Title position for gauges like cell binds.

## Impact

- Host web UI (`data/index.html`, `data/user.html`, shared CSS if needed) and scene pin JSON shape; host version **4.2.67**.
- No slave changes; no fragile SPI/OTA zones.
- Aligns with existing console binding `showTitle` / `titlePosition` semantics without changing cell-bind defaults.
