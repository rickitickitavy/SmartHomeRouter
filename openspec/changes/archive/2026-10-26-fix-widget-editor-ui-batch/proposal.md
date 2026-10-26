# Proposal

## Why

The Consoles widget editor has four operator-facing defects: Grid Columns/Rows stay visible when they should not, the add-device dialog duplicates Align H/V that composite cells already expose, composite text cells round-trip as the literal `"center"` instead of the typed text, and several enabled numeric inputs look grayed like readonly fields. Fixing them together restores trustworthy save/reload and editable chrome.

## What Changes

- Show widget **Columns** and **Rows** in the widget editor **only** when View style is **Grid** (hidden for Vertical and Composite).
- When the add/edit device dialog is opened for a **composite** device cell, **hide Horizontal align and Vertical align** (cell chrome already owns those). Vertical/grid binding edits keep the existing align controls.
- Fix composite **text cell** persistence so the operator’s typed text survives Save and reopen (today it can reload as `"center"`).
- Style enabled editor numeric inputs (row height, cell width, Columns, Rows, and similar) with the same interactive colors as dropdowns — not gray/muted chrome reserved for disabled/readonly.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: Grid Columns/Rows visibility; hide binding-dialog Align H/V for composite cell device edits; enabled numeric inputs must not look grayed.
- `control-consoles`: composite text cell display text MUST round-trip through save/load (parser must not confuse `"content":"text"` with the `text` field).

## Impact

- UI: `data/index.html`, `data/css/all.css` (and `data/user.html` only if view-mode text display needs a paired fix — expected no).
- Firmware: `include/JsonField.h` (`extractJsonString` key matching) and/or `src/ConsoleStore.cpp` composite text parse; `FIRMWARE_VERSION` propose bump `3.5.63` → `3.5.64` (apply bumps again when source lands).
- Specs: `web-console`, `control-consoles` deltas below.
