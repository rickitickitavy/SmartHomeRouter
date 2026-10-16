# Design: widget composite vertical layout

## Context

See proposal.md for motivation. Today the Consoles editor authors widgets in a multi-column CSS grid (`consoleEditorDraft.columns`), inserts `spacer: true` widgets to fill holes, and offers a four-way move pad. Widget `viewport` is `vertical` | `horizontal` | `grid` in `ConsoleStore` / UI. Binding `viewStyle` (`default` | `gauge` | `icon`) is separate and must stay unchanged. Adaptive view mode still uses narrow one-column / wide two-column capped widgets; `fullWidth` will opt a widget out of the half-row + 10 cm cap.

## Goals / Non-Goals

**Goals:**

- Single vertical authoring stack; remove spacer widgets and left/right moves.
- Drop viewport `horizontal`; add `composite` with cm/u geometry; keep Vertical and Grid behavior.
- Persist and render `fullWidth`.
- Heap-safe JSON round-trip on ESP32 host.

**Non-Goals:**

- Changing binding view styles or gauge/icon math beyond fitting composite cells.
- Redesigning the adaptive 13 cm breakpoint itself (only `fullWidth` overrides width for opted-in widgets).
- Requiring migration of every saved console by the operator (host coerces on load).
- Touching fragile OTA / SPI zones.

## Decisions

### 1. Terminology: Viewport vs binding view style

User-facing “Horizontal / Composite view style” maps to widget **Viewport** (UI label stays “Viewport”). Binding **View style** remains Default / Gauge / Icon. Specs and code keep that split so existing requirements stay coherent.

**Alternative considered:** Rename Viewport to View style in the UI — rejected to avoid colliding with binding View style.

### 2. Authoring stack replaces editor columns

- Editor grid always one column; hide or ignore console `columns` for placement (field may remain in JSON for compatibility but MUST NOT drive editor layout).
- `makeConsoleSpacerWidget` / `trimTrailingConsoleSpacers` / left-right branches in `moveConsoleWidgetDirection` and `consoleWidgetDropTargetIndex` go away.
- Reorder = splice/array move or swap with neighbor index only.

**Alternative considered:** Keep spacers but hide them — rejected; proposal removes empty widgets entirely.

### 3. Legacy horizontal and spacers

- `parseViewportId("horizontal")` → `CONSOLE_VIEWPORT_VERTICAL` (or map enum then treat as vertical on serialize).
- On parse widget list: skip `spacer: true` widgets; do not write them back.
- Binding-level spacers inside Grid (empty cells) stay as today’s grid empty-cell model unless Grid already uses binding spacers — keep Grid binding spacers as-is so Grid behavior is preserved.

### 4. Composite JSON shape (widget-level)

Prefer an explicit tree on the widget when `viewport === "composite"`:

```json
{
  "viewport": "composite",
  "fullWidth": false,
  "compositeRows": [
    {
      "height": 1,
      "heightUnit": "u",
      "cells": [
        {
          "width": 3,
          "widthUnit": "cm",
          "contour": false,
          "content": "text",
          "text": "Label"
        },
        {
          "width": 1,
          "widthUnit": "u",
          "contour": true,
          "content": "device",
          "binding": { "...existing binding fields..." }
        },
        {
          "width": 1,
          "widthUnit": "u",
          "contour": false,
          "content": "empty"
        }
      ]
    }
  ]
}
```

- `heightUnit` / `widthUnit`: `"cm"` | `"u"`.
- Device cell `binding` reuses the same schema as `devices[]` entries (including viewStyle).
- For Composite, top-level `devices` MAY be omitted or derived as a flat list of device-cell bindings for any code that still iterates bindings (group On/Off, export helpers). Prefer **derive on read** from cells so there is one source of truth.

**Alternative considered:** Keep a flat `devices[]` plus separate geometry indices — rejected; indices drift when cells are empty/text.

### 5. C++ model

- Add `CONSOLE_VIEWPORT_COMPOSITE` (reuse or retire HORIZONTAL slot value carefully: prefer new id `3`, map old `1` horizontal → vertical on parse so in-memory never stays horizontal).
- `ConsoleWidget`: `bool fullWidth`; composite row linked list (or capped arrays) with cells; each cell: width, units, contour, content kind, optional text, optional binding pointer.
- Hard caps (suggested): rows ≤ 8, cells/row ≤ 8, text ≤ existing title max — keep heap predictable.
- Serialize composite only when viewport is composite; ignore composite payload for other viewports.

### 6. Size math (CSS / JS)

Compute once per row from widget content box width in CSS px, converting cm via a shared `cmToPx` helper (same basis as the 13 cm breakpoint elsewhere):

`unitPx = (widgetWidthPx - sumCmWidthsPx) / sumUnitWeights`  
`cellWidthPx = widthUnit === "cm" ? cmToPx(width) : unitPx * width`  
`rowHeightPx = heightUnit === "cm" ? cmToPx(height) : unitPx * height`

Clamp `unitPx` to a small minimum if the formula is ≤ 0 so the UI does not collapse.

### 7. fullWidth in view mode

- CSS class on widget card when `fullWidth` (e.g. `console-widget--full-width`).
- Wide layout: full-width widgets `grid-column: 1 / -1` (or equivalent) at 100% content width; non-full-width keep half-row + max 10 cm.
- Narrow layout: already 100% width; checkbox still persists for when the console is wide.

### 8. Editor Composite UI

- Left gutter: height number + unit select per row.
- Above each cell: width number + unit select (horizontal stack matching cells).
- Cell body: content type select; text input or “Edit binding” / empty; contour checkbox.
- **Add row** appends a row with one default `1 u` empty cell (in-memory only until Save — no orphan persistence before Save).
- Adding a cell within a row: small “+” on the row (assume one control; record in tasks).

**Assumption:** Adding cells to a row uses an explicit “Add cell” on that row (not only Add row). Recorded as design default.

## Risks / Trade-offs

- **[Risk] Heap growth from nested composite JSON** → Mitigation: hard row/cell caps; refuse save with clear error when over limit or low heap (existing ConsoleStore heap checks).
- **[Risk] Dual binding lists (`devices` vs cell bindings) diverge** → Mitigation: composite uses cell bindings only; flatten for group On/Off.
- **[Risk] Operators lose Horizontal layouts** → Mitigation: coerce to Vertical (same bindings order); document in release notes.
- **[Risk] cm vs CSS px drift across devices** → Mitigation: reuse existing console cm helpers; accept small cross-device variance.
- **[Trade-off] Console `columns` becomes unused in editor** → Keep field in JSON for older clients; stop exposing as placement control if the UI still has a columns field.

## Migration Plan

1. Propose-time: `FIRMWARE_VERSION` `3.4.26`.
2. Apply: store + UI; load path coerces horizontal → vertical and drops widget spacers.
3. Flash host (consoles live on host LittleFS). No slave/SPI change.
4. Rollback: older firmware may ignore `composite` / `fullWidth` (treat unknown viewport as vertical if parse falls through); composite-only widgets would look empty/vertical until re-upgrade — acceptable for this product’s flash pairing model.

## Open Questions

- Exact default when Add row creates the first cell (one `1 u` empty cell) — confirmed above as design default; tweak during apply if UX feedback differs.
- Whether to expose “Add cell” as a button label vs “+” icon — cosmetic; decide during apply to match web-console skill patterns.
