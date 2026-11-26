## ADDED Requirements

### Requirement: Binding editor Flip control for icons
Wherever the Consoles (or scene-pin) binding editor offers icon view style, it MUST show a **Flip** select with options **Normal**, **Vertical**, **Horizontal**, and **Vert + horiz** (values `normal`, `vertical`, `horizontal`, `both`). Flip MUST be visible only when view style is `icon` (same visibility gate as Icon scale / icon picker). The control MUST default to Normal for new icon bindings. Operator Place and simple-user pages MUST stay synchronized for flipped icon display.

#### Scenario: Flip visible for icon style
- **WHEN** the editor sets view style to icon
- **THEN** the Flip select is shown with Normal selected by default for a new binding

#### Scenario: Flip hidden for non-icon style
- **WHEN** view style is default or gauge
- **THEN** Flip is not shown

#### Scenario: Simple-user matches Place for flip
- **WHEN** the same console icon binding with Flip Vertical is opened on Place and on the simple-user page
- **THEN** both surfaces show the same vertical mirror
