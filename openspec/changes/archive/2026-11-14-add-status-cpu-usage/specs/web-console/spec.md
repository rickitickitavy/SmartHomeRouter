# Spec Delta

## ADDED Requirements

### Requirement: Status shows host CPU usage
Status SHALL show host CPU busy percent(s) from the gateway status API. On dual-core hosts Status SHALL show CPU0 and CPU1. Unavailable samples SHALL display as an em dash (or equivalent), not as 0%. Operator Status and simple-user Status MUST both show these values when the viewer can open Status. Status SHALL NOT offer settings forms that write CPU configuration. This change does **not** require a Slave CPU row.

#### Scenario: Host dual-core values
- **WHEN** Status is open and the host reports CPU0 12% and CPU1 40%
- **THEN** both operator and simple-user Status show those host percents

#### Scenario: Host sample unavailable
- **WHEN** Status is open and a host CPU sample is not ready
- **THEN** that host CPU row shows as unavailable (em dash), not 0%

## MODIFIED Requirements

### Requirement: Status groups use compact name-value rows
Status SHALL render live attributes as groups on a two-column Status stack (one column when the viewport is narrow). Column 1 SHALL place **General**, then **Free memory**, then **CPU** (host busy percents), then **Historical storage** (and related storage groups). Column 2 SHALL place **Devices** at the top, then **MQTT**, then **Network**, then **Connections**, then remaining groups such as **WiFi**. Each group SHALL have a heading. Each attribute SHALL keep its name and value on a single row. Values in a group SHALL align to the right. Groups SHALL use a mindful width (content plus padding) and SHALL NOT stretch to the full main-panel width. Status SHALL NOT contain settings forms that write configuration.

#### Scenario: Grouped Status layout
- **WHEN** the operator opens Status on a wide viewport
- **THEN** column 1 shows Free memory then CPU (among its groups), and column 2 starts with Devices, each group narrower than the main panel, with names on the left and values on the right of the same row
