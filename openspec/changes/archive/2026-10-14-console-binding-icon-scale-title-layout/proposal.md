# Proposal

## Why

Icon-style bindings and dense widget layouts need finer control: operators want icons larger or smaller without fighting the widget viewport fit scale, and they want to hide or reposition each binding’s title (left/right/top/bottom) for all device and automation rows—not only icons.

## What Changes

- Add binding **icon scale** (`iconScale`, default `1`) for icon View style. This factor **multiplies** any scale already applied by the widget viewport (grid cell / horizontal / vertical fit); it MUST NOT replace that viewport scale.
- Add binding **Show title** checkbox (`showTitle`, default `true`) for **all** bindings (devices and automations).
- Add binding **Title position** (`titlePosition`: `left` | `right` | `top` | `bottom`, default `left`) for **all** bindings when the title is shown.
- Add binding **Horizontal align** and **Vertical align** (`alignH` / `alignV`, defaults `center`) for **all** device and automation bindings in every View style (Default, Icon, Gauge)—not only Icon. Legacy `iconAlign` maps to `alignH` when `alignH` is absent.
- Persist the new fields in the console store and expose them in the Consoles binding dialog; Place and user pages MUST honor them.
- No **BREAKING** API: missing fields keep today’s layout (title shown on the left; icon size = viewport fit only; content align center).

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `control-consoles`: binding fields for icon scale, show title, title position, and content horizontal/vertical align; view-mode layout and icon sizing rules.
- `web-console`: binding dialog controls and Place/user rendering for the new fields.

## Impact

- Host: `ConsoleStore` / `ConsoleBinding` persist `iconScale`, `showTitle`, `titlePosition`, `alignH`, `alignV`.
- Web UI: `data/index.html`, `data/user.html`, `data/css/all.css` (row flex/grid for title positions and content align; icon CSS transform or font-size multiply).
- Firmware version bumped for this change (build only).
