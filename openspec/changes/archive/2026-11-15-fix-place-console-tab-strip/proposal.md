# Proposal

## Why

On the operator (admin) Place console tab strip on PC, console names wrap to two lines and the tab row shows both horizontal and vertical scrollbars. That differs from the earlier single-line look and from the simple-user tab strip (minus Full screen). Operators need readable one-line tab titles without scrollbar chrome on desktop.

## What Changes

- Place console tab labels MUST stay on **one line** (`nowrap`), like the simple-user console tabs.
- Place tab strip MUST **not** show horizontal or vertical scrollbars on PC/desktop viewports.
- Place MUST **not** add a Full screen button in the tab strip (simple-user Full screen stays as today; operator Full screen remains on the Account theme row).
- Restore the earlier compact single-row tab appearance for Place without changing console content panels.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `web-console`: Operator Place console tab strip single-line labels; no PC tab-strip scrollbars; no Place Full screen control.

## Impact

- Primarily `data/css/all.css` Place / `.place-console-shell` / `.user-console-tabs` rules used by Place; possibly Place markup in `data/index.html` if class/structure needs alignment with simple-user tabs (without fullscreen chrome).
- Simple-user page keeps its Full screen chrome; do not remove it.
- Host firmware build bump for propose; no slave changes; no persisted settings.
