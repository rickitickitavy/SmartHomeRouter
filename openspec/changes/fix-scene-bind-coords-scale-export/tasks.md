## 1. Classic Icon scale

- [x] 1.1 Restore Icon scale field in the Binding dialog; show only for Icon view style on non–scene-pin bindings
- [x] 1.2 Persist and apply `iconScale` for classic/grid/composite Icon cells (fit × scale); keep Scale hidden for scene pins

## 2. Export / Import placement

- [x] 2.1 Move Export, Export scheme, and Import controls to the scene bind editor (`#sceneEditDialog`)
- [x] 2.2 Remove those controls from the plan geometry editor; keep geometry Save/Cancel

## 3. Image-relative pin coordinates

- [x] 3.1 Add bake-image content-box helper (contain letterbox) for normalize/pointer and overlay layout
- [x] 3.2 Use it in scene bind place/drag/resize and overlay positioning
- [x] 3.3 Use the same mapping for console Place and simple-user scene cells (`index.html` + `user.html` / `scene.js`)

## 4. Scene gauge text

- [x] 4.1 Increase scene pin / scene-cell gauge value text size by about 2× (CSS and any SVG font sizing)

## 5. Embed

- [x] 5.1 Re-embed web assets; confirm `FIRMWARE_VERSION_HOST` build bump for this change
