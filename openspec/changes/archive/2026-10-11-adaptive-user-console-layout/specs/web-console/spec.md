# Spec Delta

## ADDED Requirements

### Requirement: Place view-mode consoles match adaptive user layout
When the gateway console shows an assigned console in Place (or equivalent operator view mode), that view-mode surface MUST use the same full-viewport-friendly shell sizing and the same one- vs two-column widget width rules as the simple-user control page. The Consoles editor dialog MAY keep its authoring grid and saved `columns` for edit and drag-reorder.

#### Scenario: Place narrow width stacks widgets
- **WHEN** an operator opens Place view for a console whose content width is under 13 cm
- **THEN** visible widgets stack in one full-width column as on the simple-user page

#### Scenario: Editor keeps authoring columns
- **WHEN** an editor opens the Consoles edit dialog for a console saved with three columns
- **THEN** the edit grid still uses that authoring column count for layout and drag targets
