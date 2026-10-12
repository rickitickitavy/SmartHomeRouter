# Design

## Context

See proposal.md for motivation. Observed today:

- System → Hardware (`#system-tab-hardware`) is a flat stack of `.field` cards plus `.dialog-actions` inside the System tab panel; on short viewports the whole panel scrolls and Save/Reset leave the viewport.
- Automation edit dialog already has sticky footer actions (`.automation-edit-actions`) and a scrollable `.automation-edit-body`. Top fields are separate `.field` cards including Title; Condition has no enable gate; no Debug flag on `AutomationRecord`.
- Engine already logs `"Automation ran"` / timer runs; condition evaluation is in `AutomationEngine::evaluateCondition` without per-value debug.

Assumptions recorded for apply: condition-enable checkbox label **Use condition**; Debug logs go to `LOGGER` (visible on System → Log); JSON keys `useCondition` and `debug`; Title removed from UI and cleared/ignored on save while old files still parse `title`.

## Goals / Non-Goals

**Goals:**

- Hardware sticky Save/Reset with optional scroll of fields.
- Automation General group UX, Title removal, spaced sections, Use condition + Debug persistence and runtime behavior.

**Non-Goals:**

- Redesigning Maintenance or other System tabs in this change.
- A separate Debug UI pane or MQTT debug topic (logs only).
- Removing `title` from the C++ struct in a breaking SPI sense (host-only field; may keep pointer empty).

## Decisions

### 1. Hardware panel layout

Mirror the automation sticky pattern inside `#system-tab-hardware`:

- Panel: `display: flex; flex-direction: column; overflow: hidden; min-height: 0; height: 100%` (within System main scroll constraints).
- `.hardware-tab-body { flex: 1; min-height: 0; overflow: auto; }` wraps fields.
- `.hardware-tab-actions { flex: 0 0 auto; border-top; }` holds Save/Reset.

If System tab parent already scrolls the whole tab, set Hardware panel to fill available height so only the body scrolls.

### 2. Automation General container

One `.automation-general-box` (same visual language as `.automation-timer-box`): heading **General**, then Name, Group, Active, Min trigger interval (keep in General—already between Active and Timer), then Debug. Do not wrap each in `.field` card chrome; use compact rows/labels inside the box. Remove Title input and stop writing non-empty title from the dialog (save empty / omit).

### 3. Section spacing

CSS margin/gap (~1 line, e.g. `1em` / `1.25rem`) between General, Timer box, Triggered heading+editor, Condition group, Actions group.

### 4. `useCondition` flag

- Persist `useCondition` bool on `AutomationRecord` (default `true` when JSON omits it).
- UI: checkbox at top of Condition group; toggling hides `#automationConditionEditor`.
- Validation: if `!useCondition`, skip “must have condition” checks; evaluation short-circuits to true before walking the tree.
- Import-empty rules: treat disabled condition as empty for Import enablement.

### 5. `debug` flag and logging

- Persist `debug` bool (default false).
- In listener match / condition compare paths, when `automation->debug`, `LOGGER.info` lines with automation name, device/channel/cluster, live value, compare op/value, and container results as practical without flooding every poll—log on evaluation attempts that already exist today.
- No new API; Log tab already shows `LOGGER` buffer.

### 6. Compatibility

Export/settings restore include `useCondition` and `debug`. Older files without them load with defaults. Title may remain in JSON as `""` for one release.

## Risks / Trade-offs

- [Debug log volume] → Only log when `debug` is true and only on real evaluation paths (status change / timer finish), not every second of countdown.
- [Hardware height inside System] → May need parent `.section-panel` / System tab flex tweak; verify Log tab still scrolls normally.
- [Title removal] → Any external tool expecting title still sees empty string; document in tasks.

## Migration Plan

1. OTA ships UI + host logic together.
2. No manual data migration; defaults apply.
3. Rollback: previous firmware ignores unknown JSON keys if present; missing keys default safely.

## Open Questions

None blocking; checkbox label **Use condition** can be renamed at apply if the operator prefers different wording.
