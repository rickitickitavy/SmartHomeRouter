# Tasks

## 1. Store and validation

- [x] 1.1 Persist `useTimeChange` (JSON round-trip / export) on automations; default false for omitted field
- [x] 1.2 Extend `@time` conditions: `timeOfDay` (HH:MM, GREATER/EQUAL/LESS) and `dayOfWeek` (1–7 Mon…Sun); truncate legacy HH:MM:SS on load
- [x] 1.3 Enforce schedule save rules: Time change requires condition enable, ≥1 time/DOW comparison, ≥1 action; listeners optional; keep TIMER/listen rules when Time change is false

## 2. Engine

- [x] 2.1 Detect local minute boundary (sane wall clock) and evaluate active Time change automations like TIMER (no condition rising-edge requirement); honor min-trigger-interval
- [x] 2.2 Compare time-of-day and day-of-week correctly in condition evaluation; stop relying on rising-edge-only current-time poll for schedule runs

## 3. Web console

- [x] 3.1 Add Time change control to automation Trigger UI; include in save/load body and empty-form Import gating
- [x] 3.2 Condition editor: Current time → Time of day (HH:MM) / Day of week (Mon=1…Sun=7) with GREATER/EQUAL/LESS

## 4. Integration check

- [x] 4.1 Smoke-check: schedule automation (Time change + EQUAL HH:MM + action) saves without listeners and runs on the minute tick; day-of-week EQUAL works; device-triggered automations unchanged
