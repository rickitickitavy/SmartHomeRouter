## ADDED Requirements

### Requirement: Scene pin icon flip
When a scene pin uses icon view style, the pin MUST store `iconFlip` with the same values and defaults as console icon bindings (`normal`, `vertical`, `horizontal`, `both`; missing → `normal`). Console scene cells on Place and the simple-user page MUST render that pin’s icon with the same flip transform as a console icon binding.

#### Scenario: Pin horizontal flip in scene cell
- **WHEN** a Plan or Custom background scene pin has view style icon and `iconFlip` `horizontal`
- **THEN** Place and simple-user scene cells show that pin icon mirrored horizontally
