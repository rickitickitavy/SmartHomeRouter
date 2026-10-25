# Spec Delta

## ADDED Requirements

### Requirement: Automation binding control mode
An automation binding MUST persist a control mode. Missing mode MUST behave as Enable/Disable. Allowed modes: Enable/Disable (default), Run, and when the bound automation has TIMER `use` true also Start timer, Stop timer, and Restart timer. The binding dialog MUST offer these as a closed dropdown (no free typing) for the selected automation.

#### Scenario: Default Enable/Disable
- **WHEN** an editor adds an automation binding without choosing a control mode
- **THEN** view mode uses Enable/Disable (active toggle) behavior

#### Scenario: Run mode button
- **WHEN** an editor saves control mode Run on an automation binding
- **THEN** view mode shows a button that requests Run for that automation once

### Requirement: Inactive automation appearance and action flash
When a bound automation is inactive, view mode MUST show the binding name in gray and the icon at 90% opacity. After the operator activates a non–Enable/Disable control (Run or a timer op) on an **enabled** automation, the icon MUST show the ON appearance for 0.6 seconds then return. When the automation is disabled, activating that control MUST NOT flash the icon.

#### Scenario: Inactive styling
- **WHEN** a bound automation is inactive and shown with an icon
- **THEN** the name is gray and the icon opacity is 90%

#### Scenario: Flash on Run when enabled
- **WHEN** the operator clicks Run on a widget automation binding whose automation is enabled
- **THEN** the icon shows ON for 0.6 seconds

#### Scenario: No flash when disabled
- **WHEN** the operator clicks Run on a widget automation binding whose automation is disabled
- **THEN** the icon does not flash

### Requirement: Grid and Composite editor row move arrows
In the Consoles widget editor, when View style is Composite, each composite row MUST offer small up and down arrow buttons to reorder rows. First-row up and last-row down MUST be disabled or no-op. When View style is Grid, binding order MUST be changed by drag-and-drop only (no up/down arrow buttons on grid cells).

#### Scenario: Move composite row up
- **WHEN** the editor clicks the up arrow on the second composite row
- **THEN** that row swaps with the row above it in the draft

#### Scenario: Grid has no cell move arrows
- **WHEN** View style is Grid and the editor views a binding cell chrome
- **THEN** up/down arrow buttons for that cell are not shown
