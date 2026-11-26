## 1. Plan hit-test

- [x] 1.1 Rename/disambiguate the two `pointInPolygon` helpers in `data/js/scene.js` so plan fill uses the XZ helper (fix the hoist shadow that makes `findFloorIndexAt` always miss)
- [x] 1.2 Ensure fill selection updates `floorIndex` and a selection `selectedFloor()` accepts (`list: "floor"` or equivalent)
- [x] 1.3 Keep handle and wall/window/door priority over floor fill; keep top-most floor on overlap

## 2. Verify

- [x] 2.1 Confirm fill click selects floor; rib click still selects; wall click still selects wall
- [x] 2.2 Confirm host firmware build is **4.6.44** (apply bump; propose was 4.6.43)
