# Design: console widget viewport and gauges

## Context

See proposal.md — Why. Today `ConsoleWidget` stores title, showGroupOnOff, spacer, and bindings; `ConsoleBinding` stores ieee/automation, channel, title, unit, decimals. View mode in `user.html` / Place builds vertical device rows (`placeBuildDeviceRow`). Automation listen already implements device → channel → param → optional custom cluster/attrId in `index.html` (`renderAutomationListenEditor`). Consoles editor binding dialog is ieee/automation + channel/unit/decimals only.

## Goals / Non-Goals

**Goals:**
- Persist and round-trip viewport + grid size and binding param / viewStyle / gauge config through `ConsoleStore` JSON and settings export/import.
- Reuse automation listen field behavior for device bindings (no second picker paradigm).
- Client-side SVG/CSS gauges (HA-like arc/linear) that scale to cell/slot; no new server rendering.

**Non-Goals:**
- New gauge types beyond arc/linear; needle-only or history charts.
- Changing console-level authoring `columns` (widget placement on the console canvas).
- SPI, MQTT payload formats, or fragile OTA zones.
- Server-side validation of live gauge values (client clamps for draw).

## Decisions

1. **JSON shape (widget)**  
   - `viewport`: `"vertical"` | `"horizontal"` | `"grid"` (omit ⇒ vertical).  
   - `gridColumns` / `gridRows`: uint8, used when viewport is grid (default 2/2).  
   **Why:** explicit names avoid clashing with console `columns`.  
   **Alt:** nest under `layout: { mode, columns, rows }` — rejected for flatter parse matching existing widget keys.

2. **JSON shape (device binding)**  
   - Align with automation listen: `param`, optional `cluster`, `attrId`.  
   - `viewStyle`: `"default"` | `"gauge"`.  
   - `gauge`: `{ "type": "arc"|"linear", "min": number, "max": number, "ranges": [ { "top": number, "color": "#rrggbb" }, ... ] }`.  
   **Why:** same keys as listen/condition where possible; nested `gauge` keeps default bindings small.  
   **Alt:** flat `gaugeType` / `gaugeMin` on binding — rejected to avoid polluting non-gauge rows.

3. **C++ model**  
   Extend `ConsoleWidget` / `ConsoleBinding` with heap strings/ints for new fields; parse/serialize in `ConsoleStore` with defaults when absent. Keep heap reserve rules; refuse grow when heap floor hit (existing).

4. **UI reuse**  
   Factor or call the same helpers as automation: `fillDeviceSelect`, `fillAutomationChannelSelect`, `fillAutomationParamSelect`, custom visibility. Console device dialog gains those controls + viewStyle + gauge group. Widget header/settings row gains viewport + conditional Columns/Rows.

5. **Layout CSS**  
   - vertical: current column flex.  
   - horizontal: `display: flex; flex-direction: row; overflow-x: auto`.  
   - grid: CSS `grid-template-columns: repeat(N, 1fr)`; items in order; overflow past N×M continues to extra auto rows.  
   Gauge root: `width/height: 100%; max` constrained by cell; arc via SVG viewBox; linear via flex bar segments colored by ranges.

6. **Value source for gauges**  
   Resolve live value from existing status/telemetry paths using ieee + channel + param (and custom cluster/attr when set). If value missing, show empty gauge / em dash. On/Off and covering never use gauge UI even if JSON has gauge.

7. **Color ranges**  
   Store sorted by ascending `top` on save. First bottom = min; each next bottom = previous top. Gaps or unsorted input: normalize on save (sort by top). Colors as `#RRGGBB` hex strings.

## Risks / Trade-offs

- [Risk] Heap growth from longer console JSON → Mitigation: keep optional fields omitted when default; existing heap floor on create/grow.  
- [Risk] Gauge SVG cost on many cells on ESP-served static UI (client CPU) → Mitigation: simple paths, no animation loops; scale with CSS.  
- [Risk] Param/custom bindings without matching live telemetry → Mitigation: show unavailable/empty like missing devices today.  
- [Trade-off] Grid “Rows” is preferred initial height; excess bindings add rows rather than hard-clip — matches proposal overflow scenario.

## Migration Plan

- Load path: missing fields ⇒ vertical / default / no gauge.  
- Old exports import unchanged.  
- New exports include new fields.  
- Rollback: older firmware ignores unknown JSON keys if present (verify parse skips unknowns); if not, omit on write from older — new firmware must tolerate missing fields only (already).

## Open Questions

_(none deferrable — HA-like look means arc/linear with segmented colors; exact SVG polish during apply.)_
