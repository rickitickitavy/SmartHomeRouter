# Spec Delta

## ADDED Requirements

### Requirement: Place console tab names stay on one line
On the operator Place console tab strip, each console tab label SHALL render on a single line without wrapping. Tab title text MUST NOT break onto a second line inside the tab control.

#### Scenario: Multi-word console name
- **WHEN** an operator opens Place with a console named with multiple words (for example two Cyrillic words)
- **THEN** that tab’s name appears on one line

### Requirement: Place console tab strip has no PC scrollbars
On PC/desktop viewports, the operator Place console tab strip SHALL NOT show a horizontal scrollbar or a vertical scrollbar. The strip SHALL remain a single compact row matching the earlier single-line Place tab look.

#### Scenario: Many Place consoles on desktop
- **WHEN** an operator views Place on a desktop-width viewport with enough consoles that labels would previously wrap or force strip scrollbars
- **THEN** the Place tab strip shows no horizontal and no vertical scrollbar chrome

### Requirement: Place tab strip scrolls with overflow arrows
On the operator Place console tab strip, when tab labels overflow the available width, the console SHALL show narrow `<` and `>` controls before the first tab and after the last tab to scroll the strip. Those controls SHALL be hidden when scrolling is not needed (no overflow). The simple-user console tab strip SHALL NOT gain these Place overflow arrows from this requirement.

#### Scenario: Overflow shows arrows
- **WHEN** an operator opens Place and console tab labels do not fit in the strip width
- **THEN** a `<` control before the tabs and a `>` control after the tabs are available to scroll

#### Scenario: No overflow hides arrows
- **WHEN** an operator opens Place and all tab labels fit without overflow
- **THEN** the `<` and `>` tab-scroll controls are not shown

### Requirement: Place tab strip has no Full screen control
The operator Place console tab strip SHALL NOT include a Full screen button. Simple-user Full screen placement and the operator Account theme-row Full screen control remain governed by their existing requirements.

#### Scenario: Admin Place has no Full screen in tabs
- **WHEN** an admin opens Place with console tabs visible
- **THEN** the Place tab strip has no Full screen button in that strip
