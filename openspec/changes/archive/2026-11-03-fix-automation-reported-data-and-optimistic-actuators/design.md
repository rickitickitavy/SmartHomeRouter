# Design

## Context

See proposal.md — Why. Observed root cause for (1): the Automation UI already offers `reportable:<cluster>/<attribute>` via `appendReportableParams`, but host `automationParamIsKnownKey` / typed listen+condition validation only accept catalog keys (`status`, `sensorValue`, …). Failed validation surfaces as HTTP 400 *“Invalid automation (need timer, or a listener plus a complete condition value; check actions)”*. Even if validation were bypassed, `AUTOMATION_PARAM_KEY_MAX` (24) is tight for full `reportable:0xXXXX/0xXXXX` strings, and `ZigbeeSpiProxy::copyEndpointStateForParam` does not read `reportableValues` for those keys—so triggers/conditions would not resolve correctly today.

Optimistic UI (2) is client-side only: Place (`data/index.html`) and simple-user (`data/user.html`) already send `/api/devices/command` then wait for live status refresh; no firmware protocol change is required for approach A.

## Goals / Non-Goals

**Goals:**

- Treat Reported data param keys as first-class automation listen/condition bindings end-to-end (validate → persist → match → compare).
- Make actuator switcher/icon (and per-member group On/Off) feel responsive with approach A (immediate flip, ≤5s confirm, else revert), synced on both console pages.

**Non-Goals:**

- Changing Sensor value / CUSTOM / battery semantics beyond what is needed to coexist with Reported data.
- Pending-cue / spinner UX (approach B was declined).
- Changing Zigbee delivery, FIFO, or radio retry behavior.
- Optimistic feedback for non-toggle actuators that do not use switcher/icon state (e.g. covering open/close buttons) unless they already present as the same binary switcher/icon path.

## Decisions

### D1 — Represent Reported data as the existing `reportable:` param string

Keep the UI’s `reportable:0xXXXX/0xYYYY` key as the persisted `param` on listeners and comparisons (same as widget bindings). Parse cluster/attribute from the key on the host instead of inventing a parallel schema.

**Alternatives:** Force operators to use CUSTOM cluster+attrId for the same rows (worse UX; UI already offers Reported data). Store separate `reportableCluster`/`reportableAttribute` fields (more migration).

### D2 — Validation and param key length

- Add `automationParamIsReportable` (parse prefix + two tokens) and treat it like a known typed key in `validateTypedListener` / `validateTypedConditionNode`.
- Increase `AUTOMATION_PARAM_KEY_MAX` enough to store `reportable:0xXXXX/0xXXXX` without truncation (current 24 is equal to the typical key length and fails when bounded as `max-1`).
- Optionally set `hasCluster`/`hasAttrId` from the parsed key on save/parse for faster matching; if not stored, match by parsing `param` at evaluate time.

### D3 — Resolve and match from `reportableValues`

- Extend `copyEndpointStateForParam` (or a sibling) to, when `param` is reportable, copy the converted text from the cached reportable-value slot for that ieee/endpoint/cluster/attr (same store widgets use).
- Listen matching: when the listener param is reportable, require the status event’s cluster/attr (or the reportable note path) to match the parsed pair; reuse event-type change/any gating already in the engine.
- Condition compare: reuse existing `compareValues` / numeric parse path on the resolved converted text (same as CUSTOM numeric when both sides parse as numbers).

### D4 — Optimistic UI is a small client pending map (approach A)

Per ieee+channel (and control identity as needed):

1. On toggle: send command; set local override to commanded ON/OFF; re-render.
2. Start/replace a 5s timer for that key.
3. On live status update: if status **matches** override → clear pending (confirmed). If status is the opposite while still pending → **ignore** (stale echo; avoids ON→OFF→ON jerks). Unrelated non-ON/OFF updates → ignore.
4. On timeout: clear override; show last live / previous state.

Group On/Off: loop members; each member gets its own map entry and timer. Aggregate group switch reflects per-member optimistic or live states with the existing “any On ⇒ group On” rule.

**Alternatives considered:** B (pending cue)—declined; C (no flip)—declined; D (no timeout revert)—rejected because offline devices would stick forever.

### D5 — Sync surfaces

Implement the pending map helpers once conceptually in both `data/index.html` (Place) and `data/user.html`; keep behavior identical. No new API.

## Risks / Trade-offs

- [Truncated or malformed reportable keys on older saves] → Mitigation: bump key max; reject clearly invalid reportable tokens on validate; treat truncated legacy rows as invalid until re-saved.
- [False revert when mesh >5s then flip again] → Accepted under approach A; 5s is the chosen product bound.
- [Double-toggle races] → Latest command replaces pending intent for that device/channel (spec’d).
- [Reportable listen on devices that only update endpoint `state` without `reportableValues`] → Ensure report path that converts ZCL into `noteReportableValue` remains the source of truth; if a report updates only legacy state, matching may miss—verify against the existing reportable note path during apply.
- [Optimistic UI vs click-action bindings] → Only apply optimistic flip when the click sends a normal on/off (or equivalent binary) device command, not when the binding runs a custom click action.

## Migration Plan

- No settings migration file rewrite required; new saves store full reportable keys.
- Existing automations without reportable params unchanged.
- Rollback: revert firmware; old builds continue to reject reportable params as today.

## Open Questions

None material for planning; apply can confirm exact `AUTOMATION_PARAM_KEY_MAX` value and whether parse-on-evaluate alone is enough vs also persisting cluster/attrId alongside the reportable param.
