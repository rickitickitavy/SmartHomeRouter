# Proposal

## Why

Simple-user control consoles are hard to use on phones and small tablets: the shell is capped below full viewport width, and the widget grid follows a fixed saved column count that does not adapt to the console’s available width. Household users need a layout that fills the screen and stacks or pairs widgets by how wide the console actually is.

## What Changes

- Make the simple-user control page (and the same view-mode console surface elsewhere) fill **100% of the viewport** (width and height), using a mobile-safe full-viewport height unit so browser chrome does not clip content.
- Drive **view-mode** widget layout from the **console container width**, not from the persisted `columns` field alone:
  - When console width is below **13 cm** (CSS reference): all visible widgets stack in **one column at 100% width**. Hidden / spacer / display-none widgets do not affect stacking.
  - When console width is **13 cm or more**: place visible widgets in **two columns**, left-to-right then top-to-bottom. Each widget is **50% of the console row**, but **not wider than 10 cm**.
- Size view-mode console typography and touch controls so they remain readable at those physical-ish widths (base type and control chrome expressed consistently with the layout breakpoints).
- Keep editor reorder / saved `columns` for authorship; **view mode** MUST apply the responsive rules above so phones stay usable even when a console was authored with many columns.
- Adopt industry practice for the implementation (CSS **container queries** on the console panel, fluid grid with a max track size) while preserving the user’s 13 cm / 10 cm / full-viewport rules. Prefer screen units derived from those centimetre thresholds over relying on true physical centimetres (CSS `cm` on screens follows the CSS reference pixel, not a ruler).
- Firmware `FIRMWARE_VERSION` build bumped to `3.1.09` for this proposal.

## Capabilities

### New Capabilities

<!-- none -->

### Modified Capabilities

- `control-consoles`: Add view-mode requirements for full-viewport shell and width-based one- vs two-column widget layout (13 cm / 10 cm rules), including that hidden widgets do not participate.
- `web-console`: Align the Place / preview view-mode console surface with the same adaptive shell and grid rules so operators see what simple users see.

## Impact

LittleFS UI assets `data/user.html`, `data/index.html` (Place console view + any shared grid helpers), and `data/css/all.css` (shell, grid, container queries, type scale). No host API or console JSON schema change required if view mode stops applying inline `gridTemplateColumns` from saved `columns`. Editor dialog layout and drag-reorder stay column-aware for authorship. No SPI / Zigbee / MQTT behavior change.
