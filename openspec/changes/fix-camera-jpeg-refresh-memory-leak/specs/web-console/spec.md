# Spec Delta

## ADDED Requirements

### Requirement: JPEG refresh memory stays bounded
Long-running JPEG-refresh camera cells MUST NOT retain unbounded prior snapshot resources in the browser. After each successful frame swap, and when a camera cell is torn down or becomes unavailable, prior frame resources MUST be released. A console with many JPEG-refresh cells MUST remain usable for hours without crashing the browser tab from camera-image retention. Place and the simple-user page MUST share this behavior. No-flicker last-good-frame rules remain in force.

#### Scenario: Multi-camera console stays open
- **WHEN** a console with many JPEG-refresh camera cells (for example twelve) stays open for at least thirty minutes with normal periodic refresh
- **THEN** the browser tab does not crash from unbounded growth of camera image resources

#### Scenario: Prior frame released after successful swap
- **WHEN** a JPEG-refresh cell successfully loads the next snapshot and shows it
- **THEN** the previous frame’s snapshot resource is released so it is eligible for reclamation

#### Scenario: Teardown releases resources
- **WHEN** a JPEG-refresh camera cell is removed, replaced, or marked unavailable
- **THEN** its refresh timer stops and its held snapshot resources are released

#### Scenario: No stacked in-flight refreshes per cell
- **WHEN** a JPEG-refresh cell’s previous snapshot load has not finished
- **THEN** the cell does not start another snapshot load for that same cell

#### Scenario: Both surfaces stay in sync
- **WHEN** the bounded-memory JPEG refresh behavior is applied
- **THEN** Place and the simple-user page both implement it
