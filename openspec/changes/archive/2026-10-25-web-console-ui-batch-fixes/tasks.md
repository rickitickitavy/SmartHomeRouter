## 1. Artifacts and version

- [x] 1.1 Proposal includes all batch items including Grid/Composite row arrows
- [x] 1.2 Bump `FIRMWARE_VERSION` build by 1 on apply (`3.5.51` → `3.5.52`)

## 2. Web UI polish

- [x] 2.1 Add Cyan `#3b82f6` to gauge color presets
- [x] 2.2 Put Columns/Rows on one line with View style in same container; preserve grid-only visibility
- [x] 2.3 Composite Remove row → recycle-bin icon button
- [x] 2.4 Rename Add binding → Add cell
- [x] 2.5 Remove app-shell top/bottom padding/margin
- [x] 2.6 Grid/Composite: small ▲/▼ to move rows (bindings / composite rows)
- [x] 2.7 Web UI skill: recycle-bin default for remove/delete

## 3. Account / Appearance

- [x] 3.1 Remove Appearance sidebar section; move theme + Sign out to user-name account tab (gateway)
- [x] 3.2 Simple user trailing account tab with theme + Sign out
- [x] 3.3 Update role/sidebar JS that referenced Appearance

## 4. Widget automation control

- [x] 4.1 Persist `automationControl` on bindings; dialog closed dropdown of available ops
- [x] 4.2 View mode: Enable/Disable vs action button; inactive gray name + icon 90%; 0.2s ON flash
- [x] 4.3 Host API for Run + timer ops; wire Place and user.html
- [x] 4.4 ConsoleStore round-trip for `automationControl`
