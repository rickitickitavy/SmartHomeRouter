---
name: all-projects
description: >-
  Hard interaction rules that apply to every project and every conversation.
  Use always: before any edit, restore, git operation, or tool action; and
  whenever the user asks a question, requests an explanation, or seeks status.
---

# ALL PROJECTS

Applies to **all** projects. Read and follow before acting.

## Hard rules

- NEVER use git to restore any changes.
- NEVER restore a whole file without asking me first.
- NEVER EVER do an action as a reaction to any question.
- If I ask any question, you MUST only answer — no edits, no tool side effects, no restores, no git mutations.
- NEVER change, reinterpret, or replace **behavior the human has described** (product rules, trigger/condition semantics, value-must-change vs rising-edge, etc.) on your own. **Propose → describe options → ask → wait** for an explicit choice before editing code or OpenSpec that redefines that behavior. Project hooks (and the maintainer’s `~/.cursor` twin) inject and gate this (`beforeSubmitPrompt` + `preToolUse` prompt).
- NEVER propose or implement a device-command / Zigbee delivery fix for only one device kind (e.g. On/Off only) unless the human explicitly scopes it that way; default is **ALL** device kinds (see `all-device-kinds` rule).
- NEVER leave a console UI / live-value / widget fix on only the operator page (`data/index.html`) or only the simple-user page (`data/user.html`); keep both synchronized unless the human explicitly scopes to one page (see `sync-operator-simple-user-ui` rule).
- NEVER commit or push on `master`, `develop`, or `main` — even if the user explicitly asks to commit/push on the current branch or to those refs. Create or switch to a feature branch first, then commit/push there. A Cursor shell hook (`.cursor/hooks/block-master-develop-git.py`) also denies these commands; do not bypass it.
- If the current branch is `master`, `develop`, or `main` and a commit is needed: create a new feature branch, tell the user the branch name, commit there, and push that branch only.
- Every new `/opsx-propose` MUST increase the firmware version **build number** (the third `MAJOR.MINOR.BUILD` component, e.g. `0.2.5` → `0.2.6`) in that project's `FIRMWARE_VERSION` during propose, before artifacts are finished. Do not wait for `/opsx-apply`. Do not bump major or minor for this rule. Skip only when the repo has no firmware version.
- EACH TIME you update project source code because the user asked you to (implement, fix, change behavior), increase that project's `FIRMWARE_VERSION` **build** by 1 in the same request (e.g. `0.2.22` → `0.2.23`). Do not bump major or minor. Skip only when the repo has no firmware version, or when the request edits only rules, skills, docs, or OpenSpec planning with no source change. Example: user asks to change LED blink timing → edit the driver **and** bump build. `/opsx-apply` that writes source counts as a code update and MUST bump (even if propose already bumped).
- ALWAYS add any **new** entity to a list, collection, store, dropdown, cache, table, or any other place **ONLY after the first successful Save**. Add/create must open local/in-memory editor state only; Cancel discards it with no store write. Never pre-create placeholders (e.g. auto `rule-N`) and never inject an unsaved name into other UI lists. Otherwise a reload keeps the orphan and Cancel cannot remove it.

## Questions vs tasks

| User intent | Allowed |
|-------------|---------|
| Question / explanation / “why” / status | Answer only. No file, shell, or git changes. |
| Explicit task (“do X”, “fix Y”, “create Z”) | Act only within that request. |

When unsure whether the message is a question or a task, treat it as a question and answer only.

## New entity properties (self-check)

After you add any property on a persisted entity, you MUST follow its **full life cycle** before you say it is added. Persistence alone is not enough. You very often drop the property on list sync, reboot, or UI surfaces.

### Persistence

1. **Single-item save and read** — create/update one entity, persist, load that same entity back; the property round-trips.
2. **List operations** — list APIs, dumps, pull/replace, import/export, and any bulk sync keep the property (including after reboot or replacing the list from another source).

### UI (required)

Trace the property through **every screen that shows that entity**, not only the place you just edited:

- **Tables / lists** — every visual table or list row (main entity table, search/found table, and any other list of the same entity).
- **Dialogs** — add, edit, settings, confirm, and any other dialog that opens that entity (readonly or editable as the design requires).
- **Reload** — after save and after reboot, those same tables and dialogs still show the stored value, not a blank/default.

Do not claim the property is added if any table or dialog omits it, or if the UI shows a default after a correct save. Fix the gaps first.

### Readonly vs runtime (never confuse these)

- **Readonly** means the operator cannot edit the value in the UI. It does **not** mean skip save, skip list/sync, skip SPI dump, or omit the field from JSON. IEEE, classified device type, and every other non-runtime attribute MUST still persist and reload after reboot.
- **Runtime-only** fields (RSSI, online/liveness, in-flight LED, last packet time, and similar live radio/link state) MUST NOT be stored as settings.
- After you add an entity field, ask: is it settings identity or live telemetry? If it is not telemetry, it MUST round-trip through save, list/sync (including host↔slave device dump), reboot, tables, and dialogs — even when the dialog marks it readonly.
