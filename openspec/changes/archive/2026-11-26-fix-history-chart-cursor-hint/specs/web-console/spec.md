## ADDED Requirements

### Requirement: History chart cursor hint readability and edge flip
On both the operator and simple-user history chart UIs, while the pointer is over the plot area, the cursor hint MUST show the sample time and the series value (or discrete state) with **readable separate lines** (at least time on one line and value/state on another; up to three lines if date and time are split). Hint text MUST NOT render as overlapping glyphs. The hint MUST stay fully inside the chart canvas wrap: if placing it on the preferred side of the cursor would clip against a wrap edge (especially the right edge), the UI MUST place the hint on the **opposite** side of the cursor, then clamp inside the wrap if still required. Operator and simple-user pages MUST stay synchronized for this behavior.

#### Scenario: Readable multiline cursor hint
- **WHEN** the operator moves the pointer over a numeric history series
- **THEN** the cursor hint shows time and value on separate lines without overlapping glyphs

#### Scenario: Flip hint away from right edge
- **WHEN** the pointer is near the right edge of the plot so a right-of-cursor hint would be clipped
- **THEN** the hint is shown to the left of the cursor and remains fully inside the chart wrap

#### Scenario: Simple-user matches operator
- **WHEN** the same historized binding is opened on the simple-user page
- **THEN** cursor hint line layout and edge-flip placement match the operator page
