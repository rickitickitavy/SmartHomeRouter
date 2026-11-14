# Tasks

## 1. Persist and dialog

- [ ] 1.1 Stop stripping `title` / `showTitle` / `titlePosition` for gauge scene pins in pin↔binding helpers; keep Icon pins untitled; verify save/reload and scene export JSON retain gauge title fields
- [ ] 1.2 Show Show title / Title / Title position in the scene pin binding dialog when View style is Gauge (same as cell binds; checkbox square-only); hide for Icon; verify save updates the pin and designer preview

## 2. Render on Place and simple-user

- [ ] 2.1 Render gauge pin titles with `titlePosition` left/top/right/bottom on Place and simple-user scene cells (`data/index.html` and `data/user.html`); verify both pages match and Show title off shows no label
- [ ] 2.2 Confirm scene designer live pin preview uses the same title visibility/position; verify host version **4.2.67** and slave unchanged
