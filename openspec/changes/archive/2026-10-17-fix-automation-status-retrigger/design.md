# Design

## Context

See proposal.md — Why. Field log for `alarm button`: first `DRY` runs; later `DRY` matches debug but not `Automation ran` because `evaluateAndMaybeRun(..., requireConditionRisingEdge=true)` latches on `lastConditionTrue`. The human product rule is **not** a global always-re-fire and **not** only rising-edge: each trigger device gets **Event type** default **Change** (value must change) vs **Any value** (accept same-value reports and allow re-run while condition stays true).

## Goals / Non-Goals

**Goals:**
- Per-listener `eventType` persisted and enforced in the engine.
- UI: labels on the same line as listen-row controls; **Event type** last among fields.
- Default **Change** so existing automations keep value-must-change behavior; operators opt into **Any value** for leak-style re-reports.

**Non-Goals:**
- Changing condition-tree UI beyond listen rows.
- Auto-migrating `alarm button` to Any value (operator sets it).
- Removing `minTriggerIntervalSec`.

## Decisions

1. **JSON:** `eventType`: `"change"` | `"any"`; omit/`null` → `change`. UI labels **Change** / **Any value**.
2. **Engine:** When matching listeners for a status report, skip listeners with `change` if the bound value is unchanged; allow `any` through. When the evaluation was admitted by at least one `any` listener, do not require condition rising-edge to run actions. When only `change` listeners admitted a real value change, keep normal evaluate-and-run (rising-edge may still apply for current-time path only — status path after a true change typically already has rising edge when entering a new value).
3. **Replace blanket discrete re-notify** with per-listener event type (spec update).
4. **UI layout:** For each listen control, label + control on one line (Device, Channel, Field, optional cluster/attr, Event type last, then Remove).

## Risks / Trade-offs

- **[Risk] Chatty Any value listeners spam actions** → Mitigation: `minTriggerIntervalSec`.
- **[Risk] Operators expect old discrete always-renotify** → Mitigation: default Change; document Any value for re-reports.

## Migration Plan

- Host firmware + console assets. Missing `eventType` → Change. No forced rewrite of stored JSON.
- Rollback: previous image ignores the key.

## Open Questions

- None — Event type labels and default confirmed by human.
