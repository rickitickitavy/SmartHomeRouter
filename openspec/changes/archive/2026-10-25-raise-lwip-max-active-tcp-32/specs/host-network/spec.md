# Spec Delta

## Purpose

Host IP stack limits for concurrent TCP sessions shared by the gateway HTTP console and MQTT services.

## ADDED Requirements

### Requirement: Host supports at least 48 concurrent TCP sessions
The host gateway firmware MUST allow at least **48** concurrent active TCP connections system-wide. That pool MUST be shared by HTTP console traffic, WebSocket traffic when used, the local MQTT broker, and any outbound MQTT client. The build MUST NOT leave the historical Arduino/LWIP default of 16 as the effective active TCP ceiling on the host.

#### Scenario: Capacity floor
- **WHEN** the host firmware is built for the production host board
- **THEN** the configured maximum active TCP sessions is at least 48

#### Scenario: Shared pool
- **WHEN** HTTP clients and MQTT TCP clients are connected at the same time
- **THEN** they consume the same host active-TCP budget (not separate 48+48 pools)

### Requirement: Socket and listen caps match the active TCP floor
When the host raises the active TCP ceiling to at least 48, related LWIP socket and listening-TCP ceilings used by that build MUST be at least as large as the active TCP ceiling so the extra PCBs are actually usable for accept/connect.

#### Scenario: Usable sockets
- **WHEN** the host is configured for 48 active TCP sessions
- **THEN** the host LWIP max sockets and max listening TCP settings are each at least 48
