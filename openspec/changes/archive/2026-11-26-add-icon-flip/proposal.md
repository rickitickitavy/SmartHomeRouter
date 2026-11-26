# Proposal

## Why

Console (and scene-pin) icon bindings today always draw the glyph upright. Operators need to mirror icons vertically and/or horizontally (e.g. left vs right doors, inverted arrows) without picking a different MDI asset for every orientation.

## What Changes

- Add an **Flip** control wherever icon view style is available (console binding editor and scene pin icon authoring).
- Values: **Normal** (default), **Vertical**, **Horizontal**, **Vert + horiz**.
- Persist the choice on the binding/pin; missing/invalid values behave as Normal.
- All console icon rendering (operator Place, simple-user page, editor preview) MUST apply the flip when painting the icon.
- Include the field in consoles/scenes persist and in global settings export/restore (same as other binding icon fields).

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `control-consoles`: Icon bindings store and render Flip; editor control when view style is icon.
- `web-console`: Binding and scene-pin icon UIs expose Flip; Place and simple-user stay synchronized for icon display.
- `scenes`: Scene pin icon bindings persist and render Flip like console icons when shown in console scene cells.

## Impact

- UI: `data/index.html`, `data/user.html`, shared CSS if needed; scene pin icon path in scene bind dialog.
- Host: `ConsoleStore` icon config (+ scene pin JSON if pins carry the field); export/restore already ships binding JSON — ensure `iconFlip` round-trips.
- Host firmware build bumped to **4.6.50** at propose.
- No slave changes.
