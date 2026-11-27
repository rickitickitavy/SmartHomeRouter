## MODIFIED Requirements

### Requirement: Host supports at least 48 concurrent TCP sessions
The host gateway firmware MUST allow at least **48** concurrent active TCP connections system-wide. That pool MUST be shared by HTTP console traffic, WebSocket traffic when used, the local MQTT broker, and any outbound MQTT client. The ESP-IDF host build MUST NOT leave a historical low default of 16 as the effective active TCP ceiling on the host.

#### Scenario: Capacity floor
- **WHEN** the host firmware is built for the production host board
- **THEN** the configured maximum active TCP sessions is at least 48

#### Scenario: Shared pool
- **WHEN** HTTP clients and MQTT TCP clients are connected at the same time
- **THEN** they consume the same host active-TCP budget (not separate 48+48 pools)
