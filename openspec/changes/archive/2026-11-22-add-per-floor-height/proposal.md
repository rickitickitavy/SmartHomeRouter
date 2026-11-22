# Proposal

## Why

Each floor must have its own height. 

## What Changes

- Each floor has its own height. Default = current default value.
- Separate Floor height spinner, placed immediately after the Add floor button. without text labels. Only arrows means height
- Spinner enabled only when a floor is selected; shows that floor’s height; edits that floor only.
- Add floor, Add wall, Add window, Add door are common buttons (not toggleable / not outlined as selected or active).

## Capabilities

### New Capabilities
- *(none)*

### Modified Capabilities
- `scenes`: per-floor height; Floor height spinner after Add floor, selection-gated.

## Impact

- Host scene editor UI and scene model.
- No slave changes.
