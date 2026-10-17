# Spec Delta

## MODIFIED Requirements

### Requirement: Console stays usable during STA scan and MQTT reconnect

While the host performs a blocking Wi‑Fi network scan, an MQTT TCP connect/reconnect, or the post-connect remote MQTT session subscription attach for the device map, the HTTP console on port 80 and host SPI keepalive work MUST remain able to progress within a few seconds (no multi-tens-of-seconds freeze of the main service loop caused by those operations running as one uninterrupted burst on that loop). Those operations MUST NOT reboot the host via stack overflow.

#### Scenario: Status answers during MQTT reconnect

- **WHEN** the remote MQTT broker is unreachable and the host is attempting reconnect
- **THEN** `GET /api/status` still returns within a few seconds

#### Scenario: Status answers during subscription attach

- **WHEN** remote MQTT has just connected and the host is still installing device topic subscriptions for a large device map
- **THEN** `GET /api/status` still returns within a few seconds and the host does not reboot from a stack-canary panic
