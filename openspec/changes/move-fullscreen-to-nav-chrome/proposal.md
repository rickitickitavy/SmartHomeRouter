# Proposal

## Why

Full screen today lives only on the Account / user-name tab (theme row), so operators and simple users must open that tab to enter or leave full screen. The control should stay reachable from every section while remaining in the navigation chrome without covering tab labels.

## What Changes

- Move the **Full screen** control out of the Account / USER tab theme row on both the operator gateway console (`data/index.html`) and the simple-user control page (`data/user.html`).
- Place it at the **leading (left) end of the main tab/nav strip**, always visible after login for all user types (admin, operator roles, simple user).
- Keep the existing document fullscreen toggle behavior (enter/exit, icon pressed state); only relocate chrome.
- Layout MUST keep section/tab **names fully readable** — the button must not overlay or clip tab labels (reserved slot / non-overlapping flex item).
- Applies to **P4 and S3** host builds alike (shared web UI).
- Remove the duplicate Full screen button from the Account theme row so there is a single control.

## Capabilities

### New Capabilities
- *(none)*

### Modified Capabilities
- `web-console`: Full screen control placement in main nav chrome for operator and simple-user shells; always visible; must not hide tab names.

## Impact

- UI/CSS only on host web assets (`data/index.html`, `data/user.html`, `data/css/all.css`); host firmware string bumped for the shipped UI.
- No slave changes; no SPI/OTA/fragile zones.
- Host version **4.2.63** (propose); slave unchanged.
