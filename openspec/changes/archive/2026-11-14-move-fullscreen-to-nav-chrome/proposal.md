# Proposal

## Why

On the simple-user control page, Full screen today lives only on the Account / user-name tab (theme row), so a simple user must open that tab to enter or leave full screen. The control should stay reachable from every console tab while remaining in the navigation chrome without covering tab labels. The operator gateway console is **out of scope** for this change.

## What Changes

- **Scope: simple user only** (`data/user.html`). Do **not** move or remove Full screen on the operator gateway console (`data/index.html`).
- Move the **Full screen** control out of the Account / USER tab theme row on the simple-user page.
- Place it at the **leading (left) end of the simple-user tab strip** (`#consoleTabs` / equivalent), always visible after login for simple users.
- Keep the existing document fullscreen toggle behavior (enter/exit, icon pressed state); only relocate chrome on `user.html`.
- Layout MUST keep console/account tab **names fully readable** — the button must not overlay or clip tab labels (reserved slot / non-overlapping flex item).
- Applies to **P4 and S3** host builds alike (shared `data/user.html` / CSS used by that page).
- Remove the duplicate Full screen button from the simple-user Account theme row so there is a single control on that page.

## Capabilities

### New Capabilities
- *(none)*

### Modified Capabilities
- `web-console`: Full screen control placement in simple-user nav chrome only; always visible for simple users; must not hide tab names. Operator shell unchanged.

## Impact

- UI/CSS on simple-user host web assets (`data/user.html`, and `data/css/all.css` only as needed for that page); host firmware string bumped for the shipped UI.
- No operator `index.html` Full screen relocation.
- No slave changes; no SPI/OTA/fragile zones.
- Host version: bump **build** on apply from whatever is current; slave unchanged.
