# Spec Delta

## MODIFIED Requirements

### Requirement: Local broker starts only for SERVER TYPE local

The host MUST start the onboard MQTT broker only when stored SERVER TYPE is `local` and Wi-Fi has a usable IPv4 address (AP or STA). The host MUST NOT start the broker when SERVER TYPE is `disable` or `remote`. Changing away from `local` after an MQTT save MUST leave the broker not listening without requiring a router restart. Changing to `local` after an MQTT save MUST start listening when Wi-Fi has an address, without requiring a router restart.

#### Scenario: Local after Wi-Fi has an address

- **WHEN** SERVER TYPE is `local` and the host has an AP or STA IPv4 address
- **THEN** the onboard MQTT broker is listening on the stored PORT

#### Scenario: Disabled

- **WHEN** SERVER TYPE is `disable`
- **THEN** the onboard MQTT broker is not listening

#### Scenario: Remote client-only

- **WHEN** SERVER TYPE is `remote`
- **THEN** the onboard MQTT broker is not listening

#### Scenario: Leave local without restart

- **WHEN** an admin saves MQTT settings changing SERVER TYPE from `local` to `disable` or `remote`
- **THEN** the onboard MQTT broker stops listening without a router restart
