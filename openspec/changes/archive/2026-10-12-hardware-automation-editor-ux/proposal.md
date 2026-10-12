# Proposal

## Why

System → Hardware is awkward on short viewports (content and Save/Reset scroll away together), and the Automation edit dialog still exposes a redundant Title field, overcrowded top fields, and a condition editor that confuses timer-only automations. Operators also need a per-automation Debug switch to log condition and trigger values while diagnosing rules.

## What Changes

- **Hardware tab:** make the settings body optionally vertically scrollable and keep Save/Reset always visible (sticky footer), following the same sticky-actions pattern as the automation/user dialogs.
- **Automation editor — General:** remove the Title field from the dialog. Place Active, Name, and Group in one **General** group (no separate field cards per control), in the prior order Name → Group → Active. End General with a **Debug** checkbox.
- **Automation editor — spacing:** add about one blank line of vertical space between groups: General, Timer, Triggered by devices, Condition, Actions.
- **Automation editor — optional condition:** add a checkbox on the Condition group; when unchecked, hide the main condition editor (for timer-only flows). Persist that flag; when off, runtime treats the condition as true / unused for evaluation.
- **Automation Debug:** when Debug is on, the host MUST log the values involved in trigger/listener matching and condition evaluation (via the existing firmware logger / Log tab path).
- Drop Title from the operator-facing edit surface; persisted automations MAY keep an empty/ignored `title` for older files until cleaned, or clear it on save (implementation detail in design).
- Firmware `FIRMWARE_VERSION` build bumped to `3.1.13` for this proposal.

## Capabilities

### New Capabilities

<!-- none -->

### Modified Capabilities

- `web-console`: Hardware tab sticky footer + scroll; Automation edit dialog General grouping, Title removal, Condition enable checkbox, Debug control, group spacing.
- `automations`: Persist optional condition-enable and Debug flags; when condition is disabled, evaluation skips the tree; when Debug is on, log trigger/condition values.

## Impact

`data/index.html` / `data/css/all.css` (Hardware panel layout, automation edit markup/JS). Host `AutomationStore` / `AutomationEngine` (new boolean fields, JSON round-trip, debug logging). Settings export/restore and automation import/export include the new fields. No SPI / Zigbee radio protocol change.
