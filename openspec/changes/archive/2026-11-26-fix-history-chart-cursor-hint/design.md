# Design

## Context

- History chart lives in `#historyChartOverlay` on both `data/index.html` and `data/user.html`.
- Cursor hint is an HTML `#historyChartTooltip` absolutely positioned inside `.history-chart-canvas-wrap`.
- Pointer handler builds text as `time + "\n" + value` (discrete: time + label) and uses `white-space: pre-line`.
- Observed bug: wrap uses `line-height: 0` (to remove canvas baseline gap). The tip inherits that, so newline lines stack with zero height → overlapping/garbled text.
- Observed clipping: tip is placed at `x + 12` with only a weak clamp to `rect.width - 8` **before** measuring tip size, so a wide tip still overflows and is cut by `overflow: hidden` on the wrap.

## Goals / Non-Goals

**Goals**

- Readable multiline hint (date/time and value/state on separate lines).
- Tip stays fully visible inside the chart wrap by flipping to the opposite side of the cursor when the preferred side would clip.
- Same behavior on operator and simple-user pages.

**Non-Goals**

- Changing chart data, smoothing, or fetch params.
- Replacing the HTML tip with canvas-drawn text.
- Changing legend or toolbar layout beyond what tip CSS needs.

## Decisions

1. **Line-height reset** — On `.history-chart-tooltip`, set an explicit normal `line-height` (e.g. `1.25`) and keep `white-space: pre-line` (or structured block lines). Do not remove wrap `line-height: 0` (still needed for canvas).
2. **Content layout** — Keep two lines for numeric (time, then value) and discrete (time, then state label). Optionally split long locale time onto two lines (date / time) if a single time line is still cramped — prefer 2 lines minimum, 3 max.
3. **Placement algorithm** — After setting tip content and making it measurable (`hidden = false`):
   - Prefer default: to the right and slightly above the cursor (`+12`, `-12`).
   - If tip would overflow the wrap’s right edge, place to the **left** of the cursor (`cursorX - tipWidth - gap`).
   - If that still overflows left, clamp into `[pad, wrapWidth - tipWidth - pad]`.
   - Mirror the same idea for vertical overflow (prefer above; if clipped, place below; then clamp).
4. **Sync** — Edit tip CSS once in `all.css`; update `onHistoryChartPointer` in both `index.html` and `user.html` with the same placement helper.

## Risks / Trade-offs

- Measuring tip size requires it to be visible briefly; keep `pointer-events: none` so it cannot steal the cursor.
- Very long locale strings: multiline + flip should be enough; avoid shrinking font below readability.

## Open Questions

- None; screenshots define acceptance.
