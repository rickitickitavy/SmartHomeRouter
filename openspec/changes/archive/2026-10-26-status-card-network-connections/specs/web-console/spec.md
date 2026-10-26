# Spec Delta

## MODIFIED Requirements

### Requirement: Status shows counts, traffic, and version
The Status section SHALL show host online time, master and slave firmware versions, Zigbee and MQTT registered and online device counts, Zigbee and MQTT packet sent/received counts, MQTT local status, MQTT active topic count, MQTT connection count, web and MQTT connection breakdowns with inbound/outbound totals, Wi-Fi mode, BSSID, and Wi-Fi signal strength. Status SHALL obtain these values from the gateway status API. Status SHALL NOT contain settings forms that write configuration.

#### Scenario: Open Status with devices
- **WHEN** the operator opens Status and three Zigbee devices plus one MQTT device are registered with one Zigbee device online
- **THEN** Status shows Zigbee device count 3, Zigbee online 1, MQTT device count 1, MQTT online as reported, master firmware version from the running image, slave firmware version from the last slave report, and Zigbee and MQTT packet counters in Network

### Requirement: Status groups use compact name-value rows
Status SHALL render live attributes as groups on a two-column Status stack (one column when the viewport is narrow). Column 1 SHALL place online time, then **Version**, then **Network**, then **Connections**. Column 2 SHALL place **Free memory** at the top, then **MQTT**, then **Devices** after MQTT, then **WiFi** at the end. Each group SHALL have a heading. Each attribute SHALL keep its name and value on a single row. Values in a group SHALL align to the right. Groups SHALL use a mindful width (content plus padding) and SHALL NOT stretch to the full main-panel width. Status SHALL NOT contain settings forms that write configuration.

#### Scenario: Grouped Status layout
- **WHEN** the operator opens Status on a wide viewport
- **THEN** column 1 shows online time, Version, Network, and Connections, and column 2 shows Free memory, MQTT, Devices, then WiFi, each group narrower than the main panel, with names on the left and values on the right of the same row

### Requirement: Status Devices group
The Devices group SHALL show Zigbee registered count, Zigbee online count, MQTT registered count, and MQTT online count. The Devices group SHALL NOT show packets sent or packets received.

#### Scenario: Device counters
- **WHEN** three Zigbee devices are registered with one online and two MQTT devices are registered with one online
- **THEN** Devices shows Zigbee devices 3, Zigbee online 1, MQTT devices 2, and MQTT online 1

#### Scenario: No packet rows on Devices
- **WHEN** the operator opens Status
- **THEN** the Devices group has no Packets sent or Packets received rows

## ADDED Requirements

### Requirement: Simple user account tab shows Status
A simple user’s control-console page SHALL show the live Status summary on the account tab named by the signed-in user name. Status SHALL appear after Theme and Sign out on that tab. Status widgets on that page SHALL use a single column. Status SHALL obtain values from the gateway status API and SHALL NOT contain settings forms that write configuration.

#### Scenario: Simple user opens account tab
- **WHEN** a simple user opens the tab named by their user name
- **THEN** the tab shows Theme, Sign out, and the Status groups in one column below them

### Requirement: Status Network group
The Network group SHALL show Zigbee packets sent, MQTT packets sent, Zigbee packets received, and MQTT packets received. Values SHALL come from the gateway status API and SHALL NOT mix Zigbee SPI traffic with MQTT application traffic in a single counter.

#### Scenario: Separate packet counters
- **WHEN** the host has sent 4 Zigbee packets and 2 MQTT packets and received 9 Zigbee packets and 3 MQTT packets
- **THEN** Network shows Zigbee packets sent 4, MQTT packets sent 2, Zigbee packets received 9, and MQTT packets received 3

### Requirement: Status Connections group
The Connections group SHALL show Web active connections, MQTT inbound connections, MQTT outbound connections, total inbound connections, and total outbound connections. Total inbound SHALL be Web active plus MQTT inbound. Total outbound SHALL be MQTT outbound. Values SHALL come from the gateway status API.

#### Scenario: Mixed web and MQTT connections
- **WHEN** two web clients are active, three MQTT clients are connected inbound to the onboard broker, and the host MQTT client is connected outbound
- **THEN** Connections shows Web active 2, MQTT inbound 3, MQTT outbound 1, total inbound 5, and total outbound 1

#### Scenario: No MQTT traffic
- **WHEN** SERVER TYPE is disable and no web clients are active
- **THEN** Connections shows Web active 0, MQTT inbound 0, MQTT outbound 0, total inbound 0, and total outbound 0
