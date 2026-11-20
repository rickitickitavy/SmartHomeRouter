## 1. Expand picker list

- [x] 1.1 Append the operator-approved MDI names from `design.md` to `BINARY_SENSOR_ICON_CHOICES` as `mdi:…`, keeping all existing entries; dedupe (`water-pump`)
- [x] 1.2 Verify each new name exists in `@mdi/font@7.4.47`; if any are missing, stop and ask before substituting
- [x] 1.3 Confirm picker still renders via `mdi` webfont classes and existing on/off color CSS (no rasters, no CDN meta load)

## 2. Versions and smoke

- [x] 2.1 Confirm propose host bump **4.5.15**; leave slave unchanged unless apply touches slave (it should not)
- [x] 2.2 Smoke: Icon picker shows old + new icons (e.g. tulip, sprinkler, ceiling-light, curtains); on/off colors still recolor glyphs
