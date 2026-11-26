## ADDED Requirements

### Requirement: Icon flip orientation
When a binding uses view style `icon`, it MUST store `iconFlip` as `normal`, `vertical`, `horizontal`, or `both` (missing/invalid → `normal`). View mode and editor preview MUST mirror the glyph: vertical `scaleY(-1)`, horizontal `scaleX(-1)`, both both axes; `normal` unflipped. Flip MUST not replace viewport fit or existing color/scale rules.

#### Scenario: Default flip is normal
- **WHEN** an icon binding is saved without `iconFlip`
- **THEN** after reload the icon renders unflipped (Normal)

#### Scenario: Horizontal flip persists and renders
- **WHEN** an editor saves an On/Off icon binding with `iconFlip` `horizontal`
- **THEN** after reboot Place and simple-user show that icon mirrored horizontally

#### Scenario: Vert plus horiz flip
- **WHEN** an icon binding has `iconFlip` `both`
- **THEN** view mode shows the glyph flipped both vertically and horizontally
