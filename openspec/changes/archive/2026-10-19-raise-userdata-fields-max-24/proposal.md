# Proposal

## Why

User-data (`0xFC00`) devices are capped at 16 schema fields in firmware and the web console. Operators need up to 24 named fields on a single device.

## What Changes

- Raise the maximum `userDataFields` count from **16** to **24** end-to-end (firmware parse/store + console Add/normalize).
- Ensure persisted schema JSON capacity can hold 24 fields (today’s 768-byte bound is too small for long names at 24 rows).
- Document the new max in device-registry / web-console requirements.

## Capabilities

### New Capabilities

<!-- none -->

### Modified Capabilities

- `device-registry`: User-data schema max field count is 24 (was 16).
- `web-console`: User-data parameter dialog Add allows up to 24 draft/stored fields.

## Impact

- `include/UserDataPayload.h` (`kUserDataFieldMaxCount`)
- `data/index.html` (`normalizeUserDataFields`, `addUserDataFieldRow`)
- Bounded `userDataFieldsJson` copies (`768` today) in `DeviceTopicMap`, `WebConsole`, `main` — raise as needed so 24 fields persist
- Specs: `openspec/specs/device-registry`, `openspec/specs/web-console`
- Firmware version build bump (propose: `3.5.16`)
