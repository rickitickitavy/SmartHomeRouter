# Spec Delta

## ADDED Requirements

### Requirement: Scene gauge pin dialog offers title like cell binds
When the Consoles-style binding dialog is opened as a **scene pin** editor and View style is **Gauge**, the dialog MUST show **Show title** (square-only checkbox) and, when checked, **Title** text and **Title position** select with Left, Top, Right, and Bottom — the same controls and meanings as for bindings in console cells. Saving MUST store `showTitle`, title text, and `titlePosition` on the scene pin. When View style is Icon (or not Gauge), those title controls MUST stay hidden for the scene pin dialog and Save MUST NOT require them. Designer preview MUST reflect show/hide and position without sending device commands.

#### Scenario: Enable title on scene gauge
- **WHEN** the operator edits a scene gauge pin, checks Show title, sets Title to `Hall`, chooses Top, and saves
- **THEN** the pin stores `showTitle` true, title `Hall`, `titlePosition` `top`, and the scene bind preview shows that title on top

#### Scenario: Icon scene pin hides title controls
- **WHEN** the operator opens a scene pin dialog with View style Icon
- **THEN** Show title / Title position controls are not shown for that scene pin edit
