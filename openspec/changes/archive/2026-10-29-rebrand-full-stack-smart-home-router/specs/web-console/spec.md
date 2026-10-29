# Spec Delta

## ADDED Requirements

### Requirement: Console presents Mini-HA router product identity
The gateway operator console MUST present the product as a Mini-HA router in the browser document title and the sidebar brand heading. Those labels MUST NOT use a Zigbee2MQTT-only or “z2m-gateway” product title. Technical defaults that are not brand chrome (Wi-Fi SoftAP/STA network name default `z2m-gateway`, MQTT base topic `z2m`, API path names) MAY stay unchanged.

#### Scenario: Document title is router-branded
- **WHEN** an operator opens the gateway console login or main shell
- **THEN** the browser document title names the product as Mini-HA router (not `z2m-gateway`)

#### Scenario: Sidebar brand is router-branded
- **WHEN** an operator with a gateway sidebar session views the main shell
- **THEN** the sidebar brand heading names the product as Mini-HA router (not the bare label `Gateway` alone as the product name)
