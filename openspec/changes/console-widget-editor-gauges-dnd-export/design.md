# Design

## Context

See proposal.md — Why. Consoles already persist gauge `ranges` (`top` + `color`), composite rows/cells, binding/cell `alignH`/`alignV`, and `exportConsoleJson()` in `data/index.html`. Arc digit sizing lives in duplicated `applyCompositeContentFit` in `data/index.html` (Place / editor) and `data/user.html` (simple user). Gauge ranges are add/remove-only today; composite cells have no cross-row DnD; export strips non-ASCII with `/[^\w.-]+/g` so Cyrillic names become `_`.

Approved digit rule: **option B** — proportional to rendered gauge size with a small min/max clamp; Place and simple-user MUST share the same formula.

## Goals / Non-Goals

**Goals:**
- In-place edit of gauge range top and color; eight preset colors on that picker.
- Composite cell DnD within and across rows, then refresh layout heights and drop empty rows.
- Shared proportional arc digit scaling (option B) on Place and `user.html`.
- Fix visual centering for centered icons (reproduce with Котел / `mdi:engine`).
- Export filename from console name with Cyrillic→Latin translit.
- Numeric as-logical checkbox + threshold; variant B view styles; bool-like icons.

**Non-Goals:**
- New gauge types or host API changes beyond optional binding keys `asLogical` / `logicalThreshold`.
- Changing composite cm/`u` size math beyond re-running it after DnD.
- Global color presets outside the Consoles gauge range picker.
- SPI / OTA / fragile zones.

## Decisions

1. **Gauge range in-place edit**  
   Replace read-only list rows with editable top number + color (and keep Remove). Persist still sorts by ascending `top` on save.  
   **Why:** Matches operator ask; no model change.  
   **Alt:** “Edit” mode that copies into the add fields — rejected as extra clicks.

2. **Eight presets**  
   Fixed swatches next to the gauge range `<input type="color">`: white `#ffffff`, gray `#808080`, light gray `#c0c0c0`, red `#e53935`, green `#43a047`, light blue `#4fc3f7`, yellow `#fdd835`, orange `#fb8c00`. Click sets the active color used by add and by the focused range row.  
   **Why:** Explicit list from the request.  
   **Alt:** Native `datalist` only — weaker visibility on mobile.

3. **Composite cell DnD**  
   HTML5 drag on cell chrome (not on nested inputs); drop targets = cell slots and row bodies. On drop: splice cell into target row/index; delete empty source row; call existing `applyCompositeLayoutSizes` / content fit. Stored row `height` / `heightUnit` values stay unless the emptied row is removed.  
   **Why:** “Recalculate rows height” means refresh pixel layout from existing rules, not invent new auto-height.  
   **Alt:** Pointer libraries — unnecessary for editor-only DnD.

4. **Arc digit scaling (option B)**  
   Keep `valueFont = clamp(MIN, MAX, BASE * (gaugeSize / REF))` but tune BASE/REF/MIN/MAX so small top-row gauges are no longer disproportionately tiny vs large bottom-row gauges, while phone cells remain readable. Extract one shared helper or keep formulas byte-identical in `index.html` and `user.html`. Apply per gauge from its own fitted size when that improves proportion; keep per-row common sizing only if it still satisfies the proportional + Place=`user` requirement.  
   **Why:** User chose B over pure unclamped proportion.  
   **Alt:** SVG `font-size` in user units only — harder to keep Place/`user` parity with CSS overrides.

5. **Icon center fix**  
   Diagnose with exported Котел JSON (`mdi:engine`, cell `alignH` center, `showTitle` false). Likely causes: oversized icon overflow clipping, row width 100% with asymmetric glyph box, or cell vs binding align mismatch. Fix CSS/layout so the icon’s layout box is centered in the cell (glyph optical bias secondary; prefer container centering first). Apply in both Place and `user.html` paths.  
   **Why:** Spec requires visual center, not only flex class names.  
   **Alt:** Per-icon optical offsets — fragile; avoid unless container center is insufficient for `mdi:engine`.

6. **Export translit**  
   Map Cyrillic (and common extensions) to Latin in `exportConsoleJson`, then sanitize remaining unsafe characters to `_`. Fallback name `console` if empty.  
   **Why:** Current `\w` class is ASCII-only, which caused `_` for `Котел`.  
   **Alt:** Prefer NFC Unicode filenames — worse cross-OS download behavior.

7. **Numeric as-logical (folded into this change)**  
   Store `asLogical` (bool, default false) and `logicalThreshold` (number, default 0). Conversion: `value < threshold` → FALSE, else TRUE. Variant **B**: as-logical on → Default|Icon only; off → Default|Gauge. Icon display-only with yellow/muted bool colors. Shared helper in Place + `user.html`.  
   **Why:** Operator-approved.  
   **Alt:** Keep Gauge alongside Icon (variant A) — rejected.

## Risks / Trade-offs

- [Risk] Raising MIN too high makes small gauges look “all same size” → Mitigation: verify on Котел desktop + phone screenshots; keep MIN modest.  
- [Risk] Cell DnD fighting nested binding clicks → Mitigation: drag handle or drag only from cell chrome; click still opens binding.  
- [Risk] Divergent Place vs user fit copies drift again → Mitigation: identical constants/comments or one shared snippet pattern in both files.  
- [Trade-off] Preset hex values are opinionated; operators can still use the free color input.

## Migration Plan

- No host schema migration; existing console JSON loads unchanged.  
- UI-only behavior; rollback by reverting `data/index.html`, `data/user.html`, `data/css/all.css`.  
- `FIRMWARE_VERSION` already bumped to `3.5.32` at propose; apply bumps again if source changes.

## Open Questions

_(none — digit option B, as-logical variant B, and bool-like icons approved.)_
