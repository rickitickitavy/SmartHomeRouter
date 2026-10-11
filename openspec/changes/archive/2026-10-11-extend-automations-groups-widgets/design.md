# Design: extend automations (groups, widgets, start-on-boot edge)

## Context

Automations already persist on the host with TIMER (`use`, `autoRestart`, `startOnBoot`, `defaultTime`) and an active edge that starts the timer only when `autoRestart` is set. Control consoles bind devices only (`ConsoleBinding` ieee/channel/title/unit). Status Memory currently exposes **used** heap/PSRAM percentages. This change extends those surfaces without moving evaluation to the slave.

## Goals / Non-Goals

**Goals**

- Start timer on `active` FALSE→TRUE when `startOnBoot` is set (and `use` + `defaultTime` > 0).
- Persist optional `group`; filter Automation list via left group column.
- Allow console widget rows that toggle automation `active` like On/Off devices.
- Status Memory shows and names **free** RAM/PSRAM.

**Non-Goals**

- Automation conditions/actions editing from the simple-user page.
- Group hierarchy or nested folders.
- Changing SPI / fragile OTA paths.
- Renaming existing TIMER fields.

## Decisions

### 1. Active edge and `startOnBoot`

In `AutomationStore::setActive`, when transitioning to active, start countdown if `timer.use && defaultTime > 0 && (timer.autoRestart || timer.startOnBoot)`. Keep the existing ~20 s post-boot `applyStartOnBootTimers` path. Deactivating still stops the timer.

### 2. `group` storage

Add `char *group` (or bounded buffer) on `AutomationRecord`, same character rules as name (including spaces), max length aligned with title/name (e.g. `AUTOMATION_GROUP_MAX` ≈ 64). Empty/null = ungrouped. Persist in `/automations.json`, full/summary JSON, settings export. No separate groups store—groups are derived from distinct non-empty `group` values among stored automations.

### 3. Automation UI layout

Automation section: flex/grid with a narrow left list (`#automationGroupList`) and the existing table on the right. Rows: **All** | distinct groups (sorted) | **Not in group**. Selection filters client-side from the loaded list (or re-fetch; client filter is enough). Edit dialog: Group control immediately under Name—`<input list=…>` over unique groups (same pattern as discrete value combo).

### 4. Console automation bindings

Extend `ConsoleBinding` with a target kind:

- Device: existing `ieeeText` / `channel` / `unit` / `decimals`.
- Automation: `automationName` (required), optional row `title`; ieee unused.

JSON shape (backward compatible): device bindings keep `ieee`; automation bindings use `"automation":"<name>"` (or `"target":"automation","name":"..."`). Parser MUST accept old device-only arrays.

User page (`user.html`): render automation rows with the same switch chrome as On/Off; On ⇔ `active`. Toggle calls an admin/user-safe API that invokes `setActive` (reuse existing update path or small POST `/api/automations/active`). Editor dialog MUST NOT call that API while editing.

Widget **group** On/Off remains device-only (ignore automation rows).

### 5. Status free memory

Gateway status JSON: add or replace with `heapFreeKb` / `heapFreePercent` and `psramFreeKb` / `psramFreePercent` (keep or drop used fields—prefer free as canonical for UI; used MAY remain for debugging but UI MUST bind free). Labels: group **Memory** or **Free memory**; rows **RAM** / **PSRAM** showing free kB and free %.

## Risks / Trade-offs

- **Missing automation on widget**: if named automation was deleted, user page shows a dead row or hides it—prefer show title + disabled Off and skip toggle.
- **Group rename**: renaming a group string on one automation does not rename others; operators edit each row (acceptable).
- **Heap**: extra `group` pointer and longer console JSON; stay within existing console/automation caps.

## Migration

- Existing automations without `group` → ungrouped.
- Existing console JSON without automation keys → unchanged device bindings.
- No LittleFS wipe required.

## Open Questions

None blocking; defaults above apply unless review changes them.
