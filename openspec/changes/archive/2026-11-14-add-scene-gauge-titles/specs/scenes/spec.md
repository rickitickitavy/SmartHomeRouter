# Spec Delta

## ADDED Requirements

### Requirement: Scene gauge pins support optional titles
A scene pin whose view style is **Gauge** MUST support optional title fields like console cell bindings: `showTitle` (boolean; default **false** when missing on scene pins so existing scenes stay untitled), optional title text, and `titlePosition` of `left`, `top`, `right`, or `bottom` (default `left` when shown and missing/invalid). When `showTitle` is false, live and preview rendering MUST NOT show a title. When `showTitle` is true, the title MUST appear relative to the gauge per `titlePosition`. Icon-styled scene pins MUST continue to omit titles. These fields MUST round-trip through scene save/load and exports that include pins.

#### Scenario: Gauge title top
- **WHEN** an editor saves a scene gauge pin with `showTitle` true, title `Kitchen temp`, and `titlePosition` `top`
- **THEN** after reload that pin still has those fields and Place/simple-user scene views show `Kitchen temp` above the gauge

#### Scenario: Show title off
- **WHEN** a scene gauge pin has `showTitle` false
- **THEN** Place and simple-user scene views show the gauge without a title label

#### Scenario: Legacy pin without title fields
- **WHEN** a stored scene gauge pin has no `showTitle` or `titlePosition`
- **THEN** it behaves as untitled (`showTitle` false) and still renders the gauge

#### Scenario: Icon pin stays untitled
- **WHEN** a scene pin uses Icon view style
- **THEN** no pin title is shown even if title fields are present in storage

## MODIFIED Requirements

### Requirement: Scenes persist as first-class entities
The host MUST persist scenes with a unique id, name, editable scene model (geometry, colors, scene size, multi-floor heights, camera yaw/pitch), bake reference for a ≥ Full HD image, and an ordered list of pins. Each pin MUST target a device or automation, use relative coordinates in 0..1, and use Icon or Gauge styles per common console eligibility. Gauge pins MAY store optional title, `showTitle`, and `titlePosition` as specified for scene gauge titles; Icon pins MUST NOT require titles. After reboot the same scenes MUST load.

#### Scenario: Save and reload scene with pin
- **WHEN** an editor saves scene `Floor1` with a bake and a device pin at (0.4, 0.6)
- **THEN** after reboot that scene still has the model, bake, and pin at (0.4, 0.6)
