# Design

## Context

See proposal.md — Why. Scene pins use `.scene-cell-pin` / `.scene-bind-pin` with SVG `width: 100%`.

**Operator digit proportion (any pin size):**

`gauge_width / numeric_font_height = 33.0 / 5.7`

So `numeric_font_height = gauge_width * (5.7 / 33.0)`. This is **glyph HEIGHT**, not raw CSS `font-size`. No fixed screen/CSS pixel breakpoints.

**Regression lesson:** retuning cell `applyCompositeContentFit` broke Отопление. Cell Option B MUST stay untouched.

## Goals / Non-Goals

**Goals:** Scene-pin digit glyph height keeps `33/5.7` vs gauge width at every size; Place/`user.html` parity; cells unchanged; titles left at current cqmin clamp.

**Non-Goals:** Fixed screen sizes; per-breakpoint scale tables; cell Option B retune; linear gauges.

## Decisions

1. **Cell gauges: leave alone** — Option B + `.console-gauge-value-svg { font-size: 10.88px }` for non-scene paths.

2. **Scene digits: constant HEIGHT ratio** — From SVG `clientWidth` (gauge width), `targetHeight = width * (5.7/33)`. Set CSS `font-size`, measure rendered text height (`getBoundingClientRect().height`), rescale `font-size` so measured HEIGHT matches `targetHeight`. ResizeObserver on the SVG. Never `cqw` / fixed screen sizes.

3. **Scene labels:** keep current title cqmin clamp (operator: almost excellent on large pin).

4. **Hooks** — builder rAF, composite scene-pin path, SceneKit `afterPinsPainted`. Identical helpers in `index.html` and `user.html`.

5. **Flash** — gzip-embed; rebuild/flash host.

## Risks / Trade-offs

- [Risk] `getBoundingClientRect` includes extra line-box → Mitigation: two-pass correct; tune if operator still off.
- [Risk] Cell fit touched → Mitigation: do not change Option B.

## Migration Plan

- Rebuild/flash host. Host build bump only; slave unchanged.

## Open Questions

_(none)_
