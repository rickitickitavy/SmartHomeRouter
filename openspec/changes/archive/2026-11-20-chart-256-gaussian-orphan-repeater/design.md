## Context

- Chart UIs call `GET /api/history?...&maxPoints=128`; `WebConsole` defaults/clamps to ≤128.
- Smooth today: unchecked by default, samples default **5**, `historySmoothSeriesAvg` (flat mean over a window of that size). Spec forbids EMA.
- Device dialog `deviceRouter` is a **readonly** text field. Labels: `routerId` 0 → “Main router”; known peer → peer name; unknown → `#id`. Save does **not** send `routerId`; host keeps existing on edit. Commands route by `router_id` (0 = local radio).

## Goals / Non-Goals

**Goals:**

- Cap and request **256** history points for charts.
- Gaussian smooth with `N = 2U + 1` and default **U = 3**; no EMA.
- Orphan Repeater devices: editable ROUTER select (local + paired peers); Save writes `routerId`.

**Non-Goals:**

- Changing history slab size, retention, or downsample algorithm beyond raising the point cap.
- Auto radio rebind / mesh move when `routerId` changes (operator may use existing Rebind afterward).
- Making ROUTER editable when the current peer is still paired or when `routerId` is already local.
- Slave firmware changes.

## Decisions

1. **maxPoints = 256**  
   UI always requests 256; server default 256 and accept `1…256`. Reject/clamp above 256 to 256 (same pattern as today’s 128 clamp).

2. **Gaussian window**  
   User enters `U` (≥1). Real window size `N = 2U + 1`. Indices `0…N-1`, center at index `U`. Map  
   `x_i = ((i - U) / U) * (2/3)` for `U ≥ 1` so first/last sit at **x = ±2/3**, center at **x = 0**.  
   Weights `w_i = exp(-0.5 * x_i²)` (σ = 1).  
   Smoothed value at series index `j`: weighted mean of available neighbors in `[j-U, j+U]` using the corresponding `w` for each offset; renormalize by sum of weights of samples that exist (edge truncation).  
   *Why:* Matches the stated formula (U=2 → N=5) and “endpoints at 2/3 from center on X”.

3. **Default U = 3**  
   Samples input and state default to **3** (was 5). Smooth checkbox stays **unchecked** on open (only the number changes unless the operator enables Smooth).

4. **Orphan detection**  
   `routerId != 0` and no interconnect peer with that `routerId` in the current peers list (after refresh if needed). Then replace readonly text with a `<select>`: option local (`0`, label like “local” / “Main router”) plus each paired peer (`routerId` + name). Preselect current orphan id only if listed; otherwise preselect local or leave first option—**preselect local (0)** when the stale id is not among options so Save clears the dead binding unless the operator picks a peer.

5. **Save**  
   When the orphan select is shown (or whenever the select exists and is enabled), include `routerId` in POST `/api/devices`. Host already accepts `routerId` on upsert. When ROUTER stays readonly, omit `routerId` so the server keeps the existing value (current behavior).

6. **Operator-only device dialog**  
   Device edit lives on `data/index.html`. Chart changes ship on **both** `index.html` and `user.html`.

## Risks / Trade-offs

- **[Risk] Larger history JSON on AsyncTCP** → Mitigation: keep 256 hard cap; PSRAM alloc first; same tiny-task discipline.
- **[Risk] Reassigning `routerId` without radio rejoin leaves a device unreachable on the wrong mesh** → Mitigation: document operator expectation; keep Rebind control; do not invent auto-rebind.
- **[Risk] Partial Gaussian window at series edges looks different from center** → Mitigation: renormalize; acceptable for charts.

## Migration Plan

- Ship host build **4.5.13**. No settings migration.
- Existing devices keep `routerId` until an orphan Save.

## Open Questions

None blocking. Labels “local” vs “Main router”: use existing table wording **local** in the select for `0`, keep display helper consistent with Devices table.
