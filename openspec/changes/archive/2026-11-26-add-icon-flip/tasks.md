# Tasks

## 1. Persist iconFlip

- [x] 1.1 Add `iconFlip` to `ConsoleIconConfig` (and JSON parse/serialize) with values `normal`|`vertical`|`horizontal`|`both`, default `normal` — verify round-trip on console save/reload
- [x] 1.2 Persist `iconFlip` on scene pin icon JSON the same way — verify pin save/reload keeps Flip
- [x] 1.3 Confirm global settings export/restore includes the field via existing consoles/scenes slices — verify exported JSON contains `iconFlip` when set

## 2. Editor UI

- [x] 2.1 Add Flip select (Normal / Vertical / Horizontal / Vert + horiz) beside icon controls; show only when view style is icon — verify hidden for default/gauge and default Normal for new icons
- [x] 2.2 Wire Flip into console binding save/load drafts and scene-pin icon authoring — verify saved value matches select

## 3. Render (Place + simple-user)

- [x] 3.1 Apply CSS flip transform in shared icon paint helpers on operator Place (`index.html`) — verify Normal/Vertical/Horizontal/both visually
- [x] 3.2 Apply the same transform path on simple-user (`user.html`) and scene-cell pin icons — verify both pages match for the same binding/pin
- [x] 3.3 Keep editor preview synchronized with view-mode flip — verify Consoles edit preview mirrors the saved Flip

## 4. Integration check

- [x] 4.1 Smoke: icon binding Flip Horizontal + scene pin Flip Vertical on a console; reload; Place and user.html both correct — verify against specs

## Workflow follow-up

- Archive after implementation review.
- Host build **4.6.51** at apply.
