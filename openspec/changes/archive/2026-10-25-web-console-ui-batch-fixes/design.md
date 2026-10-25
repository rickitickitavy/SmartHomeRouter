# Design

## Context

See proposal.md. Host already has `/api/automations/active`, timer helpers on `AutomationStore`, and `AutomationEngine::runActions`. Widget bindings persist in `ConsoleStore`. UI lives in `data/index.html`, `data/user.html`, `data/css/all.css`.

## Goals / Non-Goals

**Goals:**
- Encode the batch UX + widget automation control mode + host Run/timer invoke path.
- Keep automation-editor inter-automation action ops unchanged.

**Non-Goals:**
- New automation-editor RUN op between automations.
- Rebrand or theme redesign beyond moving the existing theme picker.

## Decisions

1. **Control mode field** — Persist `automationControl` on automation bindings: `enableDisable` (default), `run`, `startTimer`, `stopTimer`, `restartTimer`. Missing → `enableDisable`.
2. **Host API** — Extend or add a small authenticated POST (e.g. `/api/automations/control`) with `{ name, op }` where `op` is `RUN` / timer ops; reuse store/engine helpers. Active toggle keeps `/api/automations/active`.
3. **Grid Columns/Rows layout** — Put Columns/Rows as labeled fields inside the same toolbar field container as View style, horizontal flex; hide when not grid.
4. **Account tab** — Replace Appearance sidebar button + Sign out with user-name sidebar button; panel hosts existing theme picker/save + Sign out. Simple user: trailing tab built like console tabs.
5. **Row arrows** — Reuse `▲`/`▼` styling from `console-widget-move-btn` for binding rows (grid) and composite row tools.
6. **Recycle bin** — Use existing `console-widget-delete-btn` + `🗑` for composite remove row; document in web-ui skill.

## Risks / Trade-offs

- [Risk] Prompt-based edit gates without chat context → Mitigation: removed; agent rule + inject remain.
- [Risk] Run bypasses conditions → Acceptable; operator-initiated from widget by design.
- [Risk] Inactive automation Run still fires → Matches stated Run semantics.

## Migration Plan

- Old bindings without `automationControl` load as Enable/Disable.
- No settings wipe. Deploy with normal firmware bump on apply (`3.5.51` → `3.5.52`).

## Open Questions

- None for this batch.
