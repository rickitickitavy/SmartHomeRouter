# Proposal

## Why

On **scene** arc gauges (Place and simple-user scene pins), the arc scales correctly when the viewport/resolution changes, but center **digits** and the gauge **label/title** do not keep stable proportions to the arc. Operator-approved looks:

- **Digits:** `Screenshot from 2026-11-15 21-31-18.png` (scene gauge numbers, e.g. `4.2`)
- **Label:** `Screenshot from 2026-11-15 21-32-10.png` (scene gauge label `температура на улице` under digits)

Those digit-to-arc and label-to-digit proportions MUST hold at any pin size and screen resolution.

**Out of scope:** composite/grid **cell** gauges. Those MUST keep existing Option B fit and MUST NOT be retuned by this change.

## What Changes

- Scene-pin arc **digit** font MUST match the 21-31-18 digit-to-arc proportion and keep that ratio across viewport/resolutions.
- Scene-pin gauge **label** MUST match the 21-32-10 title hierarchy (clearly smaller than digits, about one-third digit height) and scale with the pin across resolutions.
- Place and simple-user scene pin rendering stay identical.
- Composite/grid cell gauge digit/title fit is unchanged.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `scenes`: scene-pin arc digit and label sizing use approved proportions across resolutions.
- `web-console`: Place/user scene gauge digit and label sizing track pin/arc scale; cell gauges unchanged.
- `control-consoles`: cell gauges remain on Option B fit (no retune).

## Impact

- Host web UI: scene-pin digit/label sizing (CSS and/or SVG); cells unchanged.
- Host firmware embed + build bump; no slave changes.
