# Proposal

## Why

Operators need schedule-style automations (run at a time of day / on certain weekdays) without tying them to a device status change. Today Current time exists only as a condition with HH:MM:SS and rising-edge polling (GREATER/LESS only, no day-of-week), which is awkward for schedules and does not match a clear minute-tick trigger model.

## What Changes

- Add an automation **Time change** trigger that fires once per local wall-clock minute at second 00.
- Extend conditions to compare **time of day** (HH:MM) and **day of week** (Monday=1 … Sunday=7) with greater / equal / less, using host local time (Hardware UTC offset).
- Save rules: a Time change trigger alone is not enough — Save REQUIRES the Time change trigger **and** at least one complete time-of-day or day-of-week condition **and** at least one action; listening devices remain optional for that shape (like TIMER-only).
- Evolve existing `@time` Current time comparisons to HH:MM + EQUAL (keep GREATER/LESS); add day-of-week comparisons. Replace continuous rising-edge current-time polling with the minute-boundary Time change trigger path for schedule automations.
- **BREAKING** (condition value shape): Current time compare values move from `HH:MM:SS` to `HH:MM` (load path MAY accept legacy `HH:MM:SS` by truncating to minutes).

## Capabilities

### New Capabilities

### Modified Capabilities
- `automations`: Time change minute trigger; time-of-day and day-of-week conditions; save validation for schedule automations; evaluation semantics on the minute tick.
- `web-console`: Automation edit Trigger UI for Time change; Condition UI for time-of-day / day-of-week with >, =, <.

## Impact

- Host only: `AutomationStore`, `AutomationEngine`, automation JSON persist/export/restore, operator Automation editor in `data/index.html` (and any matching automation editor surfaces).
- Uses existing host local wall clock / NTP timezone; no slave changes.
- Host firmware build bump for this propose: `4.5.04`.
