# Spec Delta

## ADDED Requirements

### Requirement: Binding activation opens bound series history
In operator and simple-user console view mode, activating a device binding value control (name/title or gauge) SHALL open history for that binding’s historized series only. If the bound series is not historized, the console SHALL NOT present it as if samples existed.

#### Scenario: Click gauge opens matching reportable
- **WHEN** a household user clicks a gauge bound to reportable temperature on a device that also historizes humidity
- **THEN** the history UI shows the temperature series for that binding

#### Scenario: Same on operator Place view
- **WHEN** an operator clicks the same style of binding in Place view mode
- **THEN** the history UI opens for that bound series the same way as on the simple-user page
