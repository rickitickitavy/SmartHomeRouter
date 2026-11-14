# Spec Delta

## ADDED Requirements

### Requirement: Status shows controller CPU usage
Status SHALL show host CPU busy percent(s) from the gateway status API. On dual-core hosts Status SHALL show CPU0 and CPU1. When the host has a cached slave CPU busy percent, Status SHALL show slave CPU as well. Unavailable samples SHALL display as an em dash (or equivalent), not as 0%. Operator Status and simple-user Status MUST both show these values when the viewer can open Status. Status SHALL NOT offer settings forms that write CPU configuration.

#### Scenario: Host dual-core values
- **WHEN** Status is open and the host reports CPU0 12% and CPU1 40%
- **THEN** both operator and simple-user Status show those host percents

#### Scenario: Slave percent when linked
- **WHEN** Status is open and the host has a cached slave CPU of 55%
- **THEN** both Status views show slave CPU 55%

#### Scenario: Slave unavailable
- **WHEN** Status is open and the slave link has no CPU sample
- **THEN** slave CPU shows as unavailable (em dash), not 0%

## MODIFIED Requirements

### Requirement: Status groups use compact name-value rows
Status SHALL render live attributes as groups on a two-column Status stack (one column when the viewport is narrow). Column 1 SHALL place online time, then **Version**, then **Network**, then **Connections**. Column 2 SHALL place **Free memory** at the top, then **CPU** (host and slave busy percents), then **Historical storage**, then **MQTT**, then **Devices** after MQTT, then **WiFi** at the end. Each group SHALL have a heading. Each attribute SHALL keep its name and value on a single row. Values in a group SHALL align to the right. Groups SHALL use a mindful width (content plus padding) and SHALL NOT stretch to the full main-panel width. Status SHALL NOT contain settings forms that write configuration.

#### Scenario: Grouped Status layout
- **WHEN** the operator opens Status on a wide viewport
- **THEN** column 1 shows online time, Version, Network, and Connections, and column 2 shows Free memory, CPU, Historical storage, MQTT, Devices, then WiFi, each group narrower than the main panel, with names on the left and values on the right of the same row
