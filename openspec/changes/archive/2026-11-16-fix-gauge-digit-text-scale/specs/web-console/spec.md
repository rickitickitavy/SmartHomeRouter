# Spec Delta

## ADDED Requirements

### Requirement: Place and user scene gauge digits track pin scale
On Place and the simple-user page, scene-pin arc gauge center digits MUST keep the approved digit-to-arc proportion from `Screenshot from 2026-11-15 21-31-18.png` across viewport/resolutions. Place MUST match simple-user for the same scene layout and viewport. Composite/grid cell gauges are out of scope for this requirement.

#### Scenario: Scene digit ratio across resolutions
- **WHEN** Place shows a scene with an arc gauge pin at a large desktop size and again at a much smaller window
- **THEN** the center digits keep approximately the same visual size relative to that pin’s arc at both sizes

#### Scenario: Place matches user for scene digits
- **WHEN** the same scene layout is opened in Place and on the simple-user page at the same viewport size
- **THEN** corresponding scene gauge pins show the same center digit proportion
