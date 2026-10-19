# Proposal

## Why

Simple-user control pages show text (ON/OFF) for `userData` (0xFC00) boolean bindings configured with View style Icon, while the admin Place preview already shows the Material Design Icon. Operators expect the same icon presentation for assigned consoles.

## What Changes

- Simple-user console rendering treats `userData` field params of type `bool` with `viewStyle` `icon` the same as binary-sensor icon bindings (display the chosen MDI icon).
- Specs for user/Place rendering and icon view style cover `userData` bool fields, not only Binary / On/Off / automation targets.

## Capabilities

### New Capabilities

### Modified Capabilities

- `web-console`: User/Place icon rendering and binding dialog Icon path include `userData` bool params.
- `control-consoles`: Icon view style support includes `userData` boolean field bindings.

## Impact

- `data/user.html` binding row render path (and a small helper to resolve field type from schema).
- Admin Consoles Place path in `data/index.html` already correct; no API or persistence changes.
- Firmware build bump for the propose/apply cycle.
