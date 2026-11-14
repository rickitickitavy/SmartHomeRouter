# Tasks

## 1. Operator gateway console

- [ ] 1.1 Add Full screen icon button at the leading/top of `.sidebar-nav` in `data/index.html`, wire existing toggle/sync helpers, and remove it from the Account theme row; verify it stays visible on Status/Place/etc. and Account no longer duplicates it
- [ ] 1.2 Style the operator nav so the button does not shrink/cover tab labels (desktop sidebar + ≤600px horizontal strip); verify tab names remain fully readable while scrolling tabs if needed

## 2. Simple-user console

- [ ] 2.1 Place Full screen at the leading end of the simple-user tab strip in `data/user.html` (survive tab rebuild/`innerHTML`), remove Account theme-row instance; verify always visible without opening Account
- [ ] 2.2 Confirm shared CSS keeps the button from overlaying console/account tab names on narrow widths; verify labels remain readable

## 3. Cross-check

- [ ] 3.1 Verify enter/exit fullscreen and pressed/title state still work on both pages; confirm host version is **4.2.63** and slave version unchanged
