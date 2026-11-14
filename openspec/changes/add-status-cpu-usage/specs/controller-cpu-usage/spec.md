# Spec Delta

## Purpose

Defines idle-based CPU busy-percent telemetry for the host and slave controllers so Status can show load without instrumenting individual application features.

## ADDED Requirements

### Requirement: Host samples busy percent from FreeRTOS idle time
The host SHALL compute CPU busy percent from FreeRTOS run-time statistics as approximately 100 minus the idle-task share over a sampling window. On dual-core hosts the host SHALL report busy percent for CPU0 and CPU1 separately. The host SHALL NOT require per-feature application timers for this metric.

#### Scenario: Dual-core host reports two percents
- **WHEN** the host has completed at least one sampling window on a dual-core SoC
- **THEN** gateway status exposes distinct busy percents for CPU0 and CPU1

#### Scenario: Sample not ready
- **WHEN** Status is read before the first sampling window completes
- **THEN** host CPU fields are omitted or marked unavailable (not a fabricated 0% busy)

### Requirement: Slave samples busy percent from FreeRTOS idle time
The slave SHALL compute a single busy percent from FreeRTOS run-time statistics (idle share over a sampling window) the same way in spirit as the host. The slave SHALL refresh that value often enough to be useful on the host’s normal status poll cadence.

#### Scenario: Slave has a percent after a window
- **WHEN** the slave has completed at least one sampling window
- **THEN** the slave can report a busy percent in 0–100 for inclusion in status replies

### Requirement: Percents are whole numbers in range
Reported busy percents SHALL be integers from 0 through 100 inclusive once a sample is available.

#### Scenario: Clamp to range
- **WHEN** a sampling window yields a computed busy value
- **THEN** the published percent is an integer between 0 and 100
