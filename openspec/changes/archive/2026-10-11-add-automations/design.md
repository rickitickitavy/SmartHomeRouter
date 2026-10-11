# Design

## Context

See proposal.md — Why. Host already caches per-device channel status in `ZigbeeSpiProxy` (`noteReportTelemetry` / `noteMqttState`) and notifies UI via `notifyDeviceLiveChanged`. Device commands go through the existing `DeviceCommandFn` / `/api/devices/command` path. Consoles prove the pattern for heap LittleFS stores, empty-only dialog import, and settings export groups. Firmware at propose is `1.0.24`. No automation engine exists yet.

## Goals / Non-Goals

**Goals:**

- Host-only `AutomationStore` + evaluator: status-change triggers and optional TIMER countdown triggers.
- Reuse device-command send path; add in-process automation actions (enable/disable/timer ops).
- Admin Automation UI (list + edit dialog) matching web-console chrome; sidebar order Devices → Automation → … → Consoles → Appearance.
- Import/export (list all + per-dialog) and settings `automations` group.

**Non-Goals:**

- Slave-side evaluation or SPI sync of automations.
- New user role (v1: `isAdmin` only).
- Calendar/cron schedules or MQTT external rule engines; rising-edge latch beyond “trigger → evaluate → if true run”.
- Persisting last activation or TIMER runtime (`counting`, `timeLeft`).
- Visual node graph editor beyond nested list UI.

## Decisions

1. **`AutomationStore` on LittleFS (`/automations.json`), heap nodes, host only.**  
   Mirror `ConsoleStore`: linked list / heap vectors, heap floor before grow, max **32** automations. Identity is unique `name` (filename-safe); `title` stored for future UI use (editable now, list shows `name`).  
   Alternative: EEPROM main image — rejected (size/complexity; consoles already use LittleFS).

2. **Change detection in the status cache update path.**  
   In `noteReportTelemetry` (and MQTT note path that calls it): compare new state string to previous for that endpoint (and parsed cluster when present). Only on inequality call `AutomationEngine::onStatusChanged(ieee, endpoint, clusterOr0, newState)`. Online/offline toggles alone MUST NOT run automations.  
   Alternative: evaluate on every `notifyDeviceLiveChanged` — rejected; that fires for online window churn and same-value writes.

3. **TIMER complex attribute.**  
   Persisted: `use` (bool), `autoRestart` (bool), `defaultTime` (uint seconds, 0 = unset). Runtime RAM only: `counting` (bool), `timeLeft` (seconds).  
   Engine tick (main loop or periodic host task): for each active automation with `timer.use` and `counting`, decrement `timeLeft`; on reaching 0 set `counting=false`, then `evaluateAndMaybeRun(automation)` (same path as status-change). After that run attempt, if `autoRestart` and `defaultTime > 0`, set `timeLeft = defaultTime` and `counting = true`.  
   When `active` transitions FALSE→TRUE: if `timer.use` and `autoRestart` and `defaultTime > 0`, load `defaultTime` and start (`counting=true`). DISABLE / inactive stops countdown (`counting=false`; `timeLeft` may remain for UI or clear — clear on stop).  
   Ops: STOP TIMER → `counting=false`; START TIMER → if optional action seconds provided use them else `defaultTime` (reject/no-op if neither); RESTART TIMER → stop then start with same resolution.  
   Validation on save: if `!timer.use` require ≥1 listen entry and a root condition with ≥1 meaningful child (or equivalent “has at least one condition”); if `timer.use` allow empty listen and empty/absent condition (treat as TRUE).

4. **Evaluation semantics.**  
   Triggers (OR): matching listener status change, or TIMER expiry. For each trigger on an active automation: evaluate root condition against current cache (empty/absent condition when `timer.use` → TRUE); if true, run actions in order; set RAM `lastActivationMs`. No rising-edge memory on the condition itself — a second distinct change that still leaves the condition true runs again.  
   Cap nesting depth (e.g. 8) and children per container (e.g. 16) to bound stack/heap.

5. **Condition / action JSON shape (sketch).**  
   ```json
   {
     "name": "yard-light",
     "title": "Yard light",
     "active": true,
     "timer": {
       "use": true,
       "autoRestart": true,
       "defaultTime": 300
     },
     "listen": [{ "ieee": "...", "cluster": 6 }],
     "condition": {
       "kind": "container", "op": "AND", "not": false,
       "children": [
         { "kind": "comparison", "not": false, "ieee": "...", "channel": 1,
           "cluster": 6, "cmp": "EQUAL", "value": "ON" }
       ]
     },
     "actions": [
       { "target": "device", "ieee": "...", "channel": 1, "payload": "ON" },
       { "target": "automation", "name": "other-rule", "op": "ENABLE" },
       { "target": "automation", "name": "other-rule", "op": "START TIMER", "time": 60 }
     ]
   }
   ```
   Omit `cluster` on listen/comparison for “any / default main status”. Compare ops: EQUAL, GREATER, LESS (numeric when both sides parse as numbers, else lexicographic), CONTAINS, STARTS, ENDS (string). GREATER/LESS on non-numeric → false.  
   Device actions omit or use `"target":"device"` with ieee/channel/payload. Automation actions: `op` in ENABLE, DISABLE, STOP TIMER, START TIMER, RESTART TIMER; `time` optional on START TIMER. Reject SAVE TIMER ops against a target with `timer.use == false`. Export/API omit runtime `counting` / `timeLeft`; GET item MAY include them for live UI.

6. **APIs (admin session).**  
   - `GET /api/automations` — list summary: name, title, active, lastActivation (null if unknown); MAY include timer runtime summary (`use`, `counting`, `timeLeft`).  
   - `GET/PUT/DELETE /api/automations/item?name=` — full definition (+ runtime timer fields on GET).  
   - `POST /api/automations` — create.  
   - `GET /api/automations/store` + `POST /api/automations/restore` — export-all / import-all body.  
   Settings export/restore adds top-level `automations` array (definitions only; no lastActivation / counting / timeLeft).

7. **Edit dialog UX (large modal, Consoles-style).**  
   - Header: Name, Title, Active checkbox; toolbar Save / Cancel / Export / Import (Import disabled unless empty: no listeners, no actions, root condition empty, timer `use` false / default empty).  
   - Sections: **Timer** | **Listen** | **Condition** | **Actions**.  
   - **Timer**: use checkbox; autoRestart; defaultTime (seconds); readonly/runtime counting + timeLeft when open.  
   - **Listen**: table ieee (select registered), optional cluster, add/remove.  
   - **Condition**: recursive tree as before; allow empty tree when timer.use.  
   - **Actions**: ordered list; each row target type Device | Automation. Device: channel + payload (Manual command controls). Automation: name select + op (ENABLE/DISABLE; if selected automation uses timer, also STOP/START/RESTART TIMER; START TIMER shows optional time when target has no defaultTime). Reorder up/down.  
   - List page: table name | active | last performed; Add, Export all, Import all.

8. **Sidebar.**  
   Insert Automation button after Devices; move Appearance button after Consoles. Gate Automation with `isAdmin` like WiFi/MQTT/ZigBee. Update `showMainSection` / role hide logic.

9. **Settings restore.**  
   If `automations` key present → replace store; omit key → leave unchanged (same as consoles).

## Risks / Trade-offs

- **[Risk] Same-value filter misses cluster-tagged payloads that change encoding but not meaning** → Mitigation: compare normalized state strings already stored in cache; document raw-string equality.  
- **[Risk] Action storms** (sensor chatter / oscillating conditions / short auto-restart timers) → Mitigation: optional later cooldown; v1 relies on true *change* gate + timer period; log each activation.  
- **[Risk] Heap growth from deep trees** → Mitigation: depth/child caps + heap floor; reject save over limit.  
- **[Risk] Deleted device / automation still referenced** → Mitigation: missing status → comparison false; missing automation action target → skip/log; editor shows unavailable.  
- **[Risk] Cross-automation enable chains** → Mitigation: run actions synchronously in order; document no cycle detection in v1; depth/action count caps if needed.  
- **[Trade-off] Admin-only editing** → No `editAutomations` role in v1; can add later without storage change.

## Migration Plan

- New `/automations.json`; empty store on first boot.  
- No EEPROM migration.  
- Rollback: remove UI/API/engine; leave orphan JSON harmless.  
- Flash host with `1.0.24+`; slave image unchanged for this feature.

## Open Questions

- None material; wall-clock vs uptime for last-activation display can follow whatever Status already uses for timestamps.
