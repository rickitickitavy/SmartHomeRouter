# Spec Delta

## ADDED Requirements

### Requirement: Composite cell arc gauges keep Option B fit
Composite and grid cell arc gauges MUST keep the existing Option B center-digit fit (proportional to cell with the established min/max clamp). This change MUST NOT retune cell gauge digit or title sizing.

#### Scenario: Heating grid cells unchanged
- **WHEN** an operator opens a composite/grid console such as Отопление with many cell arc gauges
- **THEN** center digits and titles keep the pre-change cell fit look (not oversized into the needle/arc)
