# Spec Delta

## ADDED Requirements

### Requirement: Opposite On/Off reports reach the host
When the slave receives an On/Off (`0x0006` / `0x0000`) attribute report for a usable device endpoint, it SHALL forward that report to the host as `ON` or `OFF` matching the value when it differs from the last value forwarded for that IEEE and endpoint. Within the status-hold window, the slave MAY suppress only a report whose value equals that last forwarded value. The slave MUST NOT suppress a report solely because it differs from the last forwarded value.

#### Scenario: Rapid physical ON then OFF
- **WHEN** a registered `onOff` device reports On/Off `1` then, within a few seconds, reports On/Off `0` on the same endpoint
- **THEN** the host receives both an `ON` report and an `OFF` report for that IEEE and endpoint

#### Scenario: Repeated toggles keep publishing both sides
- **WHEN** that same endpoint reports `1`, `0`, `1`, `0` in succession within the status-hold window
- **THEN** the host receives `ON`, `OFF`, `ON`, `OFF` in that order

#### Scenario: Duplicate same status may be suppressed
- **WHEN** that same endpoint reports On/Off `1` and then reports On/Off `1` again within the status-hold window with no intervening opposite value
- **THEN** the slave MAY omit forwarding the second `ON` to the host
