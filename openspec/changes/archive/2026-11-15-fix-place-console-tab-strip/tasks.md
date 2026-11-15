# Tasks

## 1. Place tab strip CSS / structure

- [x] 1.1 Make Place console tab labels `nowrap` / single-line under `.place-console-shell` (or equivalent) so multi-word names stay on one line; verify with a two-word Cyrillic console name on desktop
- [x] 1.2 Remove horizontal and vertical scrollbars from the Place tab strip on PC/desktop (Place-scoped overflow); verify the admin multi-tab case no longer shows H/V scrollbar chrome
- [x] 1.3 Confirm Place tab strip has no Full screen control; verify simple-user Full screen chrome and operator Account Full screen remain unchanged

## 2. Regression check

- [x] 2.1 Confirm simple-user tab labels remain single-line and Full screen still works; verify Place and simple-user do not regress each other’s tab chrome
- [x] 2.2 Spot-check Place console panel content still switches correctly when selecting tabs after the strip CSS change

## Workflow follow-up

- Archive after review when implementation is complete.
