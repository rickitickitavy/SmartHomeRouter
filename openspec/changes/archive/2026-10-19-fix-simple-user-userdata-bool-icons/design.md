# Design

## Context

Admin Place rendering in `data/index.html` already draws Material Design Icons for `userData` bool params when `viewStyle` is `icon` (via `userDataFieldTypeForParam`). The simple-user page `data/user.html` only treated binary-sensor device types as icon-eligible, so the same saved binding fell through to a text plate showing ON/OFF.

## Goals / Non-Goals

**Goals:**
- Simple-user `buildBindingRow` uses the same icon eligibility rule as Place for `userData` bool fields.
- Specs document that Icon view style applies to `userData` bool params on dialog, Place, and simple-user pages.

**Non-Goals:**
- Making `userData` bool icons interactive (they stay display-only, like binary sensors).
- Changing persistence, MQTT, or 0xFC00 cluster encoding.
- Offering Icon for `userData` int/float/string params.

## Decisions

- Reuse the Place condition: `viewStyle === "icon"` and (`isBinarySensorDeviceType` or `userData` + field type `bool`).
- Add `userDataFieldTypeForParam` on the user page (same lookup as admin) so eligibility does not depend on ad-hoc param name lists.
- Keep ON/OFF token mapping in `userDataSlotDisplayToken` so icon on/off coloring still works from brace state.

## Risks / Trade-offs

- Simple-user device list must include `userDataFields` (already returned by `/api/devices`); if schema were stripped, Icon would fall back to text — acceptable and matches missing-type behavior.
