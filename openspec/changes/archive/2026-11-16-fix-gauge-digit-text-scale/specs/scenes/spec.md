# Spec Delta

## ADDED Requirements

### Requirement: Scene pin arc digit and title proportions
Scene-pin arc gauges MUST use the operator-approved digit-to-arc proportion from `Screenshot from 2026-11-15 21-31-18.png` and, when a title is shown, the label hierarchy from `Screenshot from 2026-11-15 21-32-10.png` (label clearly smaller than digits, about one-third digit height). Those proportions MUST hold as the scene/pin scales across viewport/resolutions.

#### Scenario: Matches approved scene digit look
- **WHEN** Place shows a scene pin arc gauge at a normal desktop size
- **THEN** the center digits match the visual proportion of `Screenshot from 2026-11-15 21-31-18.png`

#### Scenario: Matches approved scene label look
- **WHEN** Place shows a titled scene pin arc gauge (e.g. `температура на улице`) at a normal desktop size
- **THEN** the label matches the hierarchy of `Screenshot from 2026-11-15 21-32-10.png`

#### Scenario: Stable across resolutions
- **WHEN** the same titled scene pin gauge is viewed at a large desktop window and at a much smaller window
- **THEN** digit-to-arc and label-to-digit proportions stay at the approved looks while the arc scales
