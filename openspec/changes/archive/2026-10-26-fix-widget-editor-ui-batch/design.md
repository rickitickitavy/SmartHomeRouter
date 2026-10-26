# Design

## Context

See proposal.md for motivation. Observed code:

- Widget toolbar already sets `colsField.hidden` / `rowsField.hidden` from View style, but CSS forces `display: inline-flex` on `.console-widget-grid-inline` without a `[hidden]` override, so Columns/Rows can remain visible for Vertical/Composite.
- Composite cells already expose Align H / Align V in cell chrome; the shared binding dialog (`#consoleDeviceDialog`) still shows binding Align H/V, including when opened via `openConsoleCompositeDeviceDialog`.
- Composite text bug: `extractJsonString` in `include/JsonField.h` uses `strstr("\"text\"")`, which matches the **value** of `"content":"text"`. The next `:` then pulls the following string (often `"center"` from `alignH`). Client key order puts `content` before `alignH` before `text`, so Save → reload shows `center`.
- Selects in `#consoleEditDialog` get explicit `--text` / `--bg` / `--border`; many enabled number inputs (grid Columns/Rows, composite height/width) only get sizing rules and look grayed vs dropdowns.

`FIRMWARE_VERSION` propose bump: `3.5.63` → `3.5.64`.

## Goals / Non-Goals

**Goals:**

- Reliable Grid-only visibility for Columns/Rows.
- No duplicate align UI when editing a composite device cell.
- Correct composite text round-trip for any JSON key order.
- Enabled widget-editor numeric inputs match dropdown interactive colors.

**Non-Goals:**

- Removing Align H/V from Vertical/Grid binding edits.
- Changing composite cell align semantics or binding align persistence for non-composite widgets.
- Broad rewrite of all JSON helpers beyond fixing key-vs-value matching for string extract.

## Decisions

1. **Columns/Rows visibility**  
   Keep the existing `hidden` toggle; add CSS so `[hidden]` wins over `display: inline-flex` (e.g. `.console-widget-grid-cols-field[hidden], .console-widget-grid-rows-field[hidden] { display: none !important; }`). Re-check after View style change via full `renderConsoleEditor` (already done).  
   *Alt considered:* remove fields from DOM when not Grid — more churn; CSS + `hidden` is enough.

2. **Binding dialog align vs composite cell**  
   When `consoleCompositeEditTarget` is set (composite device path), hide Horizontal and Vertical align fields for the whole dialog open. Vertical/Grid opens keep today’s Show-title rules. Binding may still store default `alignH`/`alignV`; layout for composite uses cell align.  
   *Alt considered:* remove align from the dialog globally — rejected; Vertical/Grid have no cell chrome for align.

3. **Fix `extractJsonString` key matching**  
   After finding `"key"`, require optional whitespace then `:`; if not, continue searching. That skips `"content":"text"`’s value and finds the real `"text":"…"`. Prefer fixing `extractJsonString` (shared) over renaming the JSON field. Verify with the failing key order in a quick host-side or manual Save/reload.  
   *Alt considered:* only use `extractJsonStringRoot` in `parseCompositeRows` — narrower; same class of bugs can hit other keys elsewhere.

4. **Input colors**  
   Add `#consoleEditDialog` rules for enabled `input[type="number"]` / text used by grid size and composite height/width (and reuse existing composite-text interactive colors) mirroring `#consoleEditDialog select`: `color: var(--text); background: var(--bg); border: 1px solid var(--border)`. Keep gray mix only for `:disabled` / `:read-only`. Aligns with project rule “enabled controls are not gray”.

## Risks / Trade-offs

- [Risk] Changing `extractJsonString` affects many parsers → Mitigation: only tighten match to `"key"\s*:`; do not change value decoding; smoke other string fields if time allows.
- [Risk] Hiding binding align in composite dialog surprises operators who set binding align before → Mitigation: cell Align H/V remain visible and are the intended controls.
- [Risk] CSS `[hidden]` override misses a class → Mitigation: target both grid-cols/rows fields and, if needed, any parent that incorrectly forces display.

## Migration Plan

- No stored-schema migration. Consoles already saved with wrong text may still show `center` until the operator re-enters text and Saves under fixed firmware.
- Rollback: revert UI/CSS/JsonField changes and firmware build.

## Open Questions

- None deferrable; composite-only hide of dialog align is the assumed reading of “cell already has these aligns.”
