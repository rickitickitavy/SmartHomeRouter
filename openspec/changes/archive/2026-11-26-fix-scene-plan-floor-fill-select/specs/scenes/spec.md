## ADDED Requirements

### Requirement: Plan editor selects floor by polygon fill click
In the Plan scene geometry editor, a pointer press on the **filled interior** of a floor polygon MUST select that floor (focused floor and floor selection used by Floor height and related chrome). Selection MUST NOT require clicking only a polygon edge or corner. When multiple floor plates overlap under the pointer, the top-most drawn floor MUST win. Wall/window/door segment hits and geometry handles MUST still take priority over floor fill when the press is on those targets.

#### Scenario: Click floor fill selects floor
- **WHEN** the operator presses inside a floor polygon’s filled area (away from handles and wall/opening segments)
- **THEN** that floor becomes the selected/focused floor

#### Scenario: Click floor edge still selects floor
- **WHEN** the operator presses near a floor polygon edge (“rib”)
- **THEN** that floor is selected as today

#### Scenario: Wall segment still wins over fill
- **WHEN** the operator presses on a wall segment that lies over a floor plate
- **THEN** the wall is selected, not the floor fill
