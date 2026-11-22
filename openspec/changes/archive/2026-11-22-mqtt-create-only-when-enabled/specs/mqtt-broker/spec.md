## MODIFIED Requirements

### Requirement: Local broker starts only for SERVER TYPE local
The host MUST construct and start the onboard MQTT broker only when stored SERVER TYPE is `local` and Wi-Fi has a usable IPv4 address (AP or STA). The host MUST NOT keep a broker instance when SERVER TYPE is `disable` or `remote` (not merely leave it not listening). Changing away from `local` after an MQTT save MUST destroy the broker (and leave no listen socket) without requiring a router restart. Changing to `local` after an MQTT save MUST create the broker and start listening when Wi-Fi has an address, without requiring a router restart.

#### Scenario: Local after Wi-Fi has an address
- **WHEN** SERVER TYPE is `local` and the host has an AP or STA IPv4 address
- **THEN** the onboard MQTT broker is listening on the stored PORT

#### Scenario: Disabled
- **WHEN** SERVER TYPE is `disable`
- **THEN** the onboard MQTT broker is not listening and no broker instance is kept

#### Scenario: Remote client-only
- **WHEN** SERVER TYPE is `remote`
- **THEN** the onboard MQTT broker is not listening and no broker instance is kept

#### Scenario: Leave local without restart
- **WHEN** an admin saves MQTT settings changing SERVER TYPE from `local` to `disable` or `remote`
- **THEN** the onboard MQTT broker is destroyed (not listening) without a router restart
