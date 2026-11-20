## Why

The console binding Icon picker exposes only a small hardcoded MDI list (~40). The operator chose a concrete set of additional Material Design Icons to add. Icons must stay **recolorable** via existing on/off color controls (MDI webfont / CSS `color`).

## What Changes

- Keep today’s picker icons and **add the operator-approved MDI names** (listed in `design.md`): garden/outdoor, lighting, water, security/gates, home appliances, etc. (~78 requested names; `water-pump` already present).
- Store/select as `mdi:…` and render with the existing `@mdi/font` webfont so on/off colors still apply.
- Do **not** load the full ~7k MDI catalog from CDN meta and do **not** add fixed-color raster icons.
- Optional small filter in the picker is allowed for convenience; not required for CDN meta.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: Expanded Icon picker catalog = current icons ∪ operator-approved list; colorability unchanged.

## Impact

- Operator console UI (`data/index.html` `BINARY_SENSOR_ICON_CHOICES` / icon grid). Live Place / simple-user already render stored `mdi:…` ids.
- Host firmware build bump only (`FIRMWARE_VERSION_HOST` → **4.5.15**). No slave change. No new CDN meta dependency.
