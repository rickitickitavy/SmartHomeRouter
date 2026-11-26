# Proposal

## Why

Operators need a scene background that is neither a generated Plan bake nor a live Camera feed — a floor plan photo, schematic, or other still image they already have. Today the only pin-authoring background is Plan’s bake-from-geometry path.

## What Changes

- Add scene kind **Custom background** (`custom`) alongside Plan and Camera.
- Custom background scenes author **exactly like Plan** for pins (icon/gauge pins on a still image) with one difference: the background comes from an **operator-uploaded** image instead of a geometry bake.
- Widen the scene kind selector to about **twice** its current width so the new label fits.
- Show an **Upload image** button only when kind is Custom background (hidden for Plan and Camera).
- Hide **Edit scene** (geometry) for Custom background — there is no plan bake to regenerate.
- Store the uploaded image on the **SD card** (not LittleFS). Upload fails with a clear error when SD is missing or unusable.
- Include custom background image bytes in **Full backup** / **Full restore** (alongside Plan bake JPEGs). Ordinary settings export keeps metadata only (path/hasBake), same as Plan bakes.
- Accept **PNG** and **JPEG** (and similar lightweight still formats the host can serve), max **1 MiB** per image.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `scenes`: New kind `custom`; pin authoring like Plan; SD-backed background image; upload size/format limits; no geometry editor for this kind.
- `web-console`: Scene kind selector option + width; Upload image chrome; Full backup/restore includes custom background files from SD; bake GET serves custom images from SD when kind is `custom`.
- `sd-card-storage`: Custom scene backgrounds live under an SD directory; mount-on-demand for upload/serve/backup consistent with existing SD lifecycle.

## Impact

- UI: `data/index.html` scene dialog chrome; `data/js/scene.js` kind helpers / live view; `data/css/all.css` kind select width; Place and simple-user scene cells for custom backgrounds.
- Host: `SceneStore` kind + path; bake/background upload & GET (LittleFS vs SD by kind); `FullBackup` pack/restore of SD custom images; possibly `SdCardStorage` helpers for mount-around-IO.
- Host firmware build bumped to **4.6.45** at propose.
- No slave changes.
