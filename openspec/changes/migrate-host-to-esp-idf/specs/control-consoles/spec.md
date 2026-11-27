## MODIFIED Requirements

### Requirement: Console data is heap-grown with Wi-Fi headroom
Console objects, widget lists, device bindings, and each user’s assigned-console list MUST be allocated at runtime (no compile-time arrays of consoles, widgets, or bindings). The host MUST refuse a create or grow that would leave too little free heap for the HTTP console server and Wi‑Fi, and MUST free heap when a console, widget, binding, or assignment is removed.

#### Scenario: Grow then shrink
- **WHEN** an editor adds a widget and later deletes that widget and saves
- **THEN** that widget is gone after reload and its heap is released

#### Scenario: Low heap refuses grow
- **WHEN** free heap is at or below the reserved Wi‑Fi/HTTP floor and the editor adds a widget
- **THEN** the host does not add the widget and shows an error
