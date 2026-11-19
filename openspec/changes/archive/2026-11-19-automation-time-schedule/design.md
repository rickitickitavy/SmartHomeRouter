# Design

## Context

See proposal.md — Why. Grounding in code:

- Automations already support Current time as condition ieee `@time` (`AUTOMATION_IEEE_CURRENT_TIME`) with `HH:MM:SS`, GREATER/LESS only, and `evaluateCurrentTimeAutomations()` on each engine tick using **rising-edge** of the condition.
- TIMER `use` already allows no listeners; validation special-cases `conditionUsesCurrentTime`.
- Host local wall clock uses Hardware `timeZoneUtcOffsetMin` + NTP (`configTime` / `localtime_r`).
- Operator editor: `data/index.html` Current time option in condition device select; Trigger section has TIMER then listen rows.

## Goals / Non-Goals

**Goals:**

- Persist a **Time change** trigger; fire once per local minute at `:00`.
- Conditions: time of day `HH:MM` and day of week Mon=1…Sun=7 with `>`, `=`, `<`.
- Save: Time change + ≥1 time/DOW condition + ≥1 action; listeners optional.
- UI: Trigger checkbox; Condition Current time → Time of day / Day of week.

**Non-Goals:**

- Calendar dates, holidays, sunrise/sunset, IANA zones beyond existing UTC offset.
- Slave-side scheduling.
- Changing TIMER countdown semantics.
- SoftAP / no-NTP scheduling guarantees (sane wall clock required).

## Decisions

### 1. Persist `useTimeChange` (name TBD in store JSON as `timeChange` / `useTimeChange`)

Boolean on `AutomationRecord`, default false. Parallel to TIMER `use`, not a fake listen row. UI checkbox in Trigger after TIMER block.

**Alternatives:** Synthetic listener ieee — rejected (user asked for a trigger; listen rows are device-oriented).

### 2. Minute tick evaluation = TIMER-style (not rising-edge)

On each local minute boundary, for active automations with Time change true: call the same evaluate-and-run path used for TIMER finish (`requireConditionRisingEdge = false`). Conditions that stay true across minutes **re-fire every minute** (e.g. `timeOfDay GREATER 08:00` AND `LESS 09:00` runs up to 60 times per hour unless min-trigger-interval limits it). `EQUAL 08:00` runs once that hour.

Replace / stop using rising-edge-only `evaluateCurrentTimeAutomations` for schedule runs so EQUAL works and schedule windows behave predictably.

Detect minute boundary via `localtime_r` minute change (or `tm_sec == 0` with last-fired minute key), not `millis` alone.

### 3. Current time params: `timeOfDay` and `dayOfWeek`

Keep ieee `@time`. Require `param`:
- `timeOfDay` — value `HH:MM`; ops GREATER/EQUAL/LESS; compare minutes-since-midnight.
- `dayOfWeek` — value `"1"`…`"7"` (Mon…Sun); ops GREATER/EQUAL/LESS; map `tm_wday` (Sun=0) → ISO-like Mon=1…Sun=7.

UI: when Current time selected, show param picker + appropriate value control (time input or weekday select). EQUAL always offered.

**Migration:** Load `HH:MM:SS` → truncate to `HH:MM`. Missing param on old `@time` rows → treat as `timeOfDay`.

### 4. Save validation

When `useTimeChange`:
- `useCondition` must be true
- condition tree must contain ≥1 complete `@time` comparison with `timeOfDay` or `dayOfWeek`
- ≥1 action
- listeners optional; TIMER `use` may still be true (orthogonal)

When not `useTimeChange`, keep existing TIMER / listen rules (update the “current time alone without listeners” shortcut if it conflicted — schedule path goes through Time change flag).

### 5. Export / global settings

`useTimeChange` is persisted automation config → included in automation store export and global settings export/restore automatically with the automations slice (no Wi-Fi exclusion).

## Risks / Trade-offs

- [Windows re-fire every minute] → By design; operators use EQUAL or min-trigger-interval to throttle.
- [Clock jump / DST not modeled] → Fixed UTC offset only; document.
- [Tick skew if loop blocked >1s] → May miss a boundary; next minute recovers; acceptable for home scheduling.

## Migration Plan

1. Deploy host + UI together.
2. Existing `@time` HH:MM:SS conditions load as HH:MM; add EQUAL in UI.
3. Operators who relied on rising-edge poll without Time change should enable Time change and keep conditions (or keep device listeners).

## Open Questions

None — operator choices locked: HH:MM; Mon=1…Sun=7; save = Time change + time/DOW condition + actions.
