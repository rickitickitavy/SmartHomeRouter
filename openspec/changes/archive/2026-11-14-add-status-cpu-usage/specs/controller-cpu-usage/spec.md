# Spec Delta

## Purpose

Defines idle-based CPU busy-percent telemetry for the **host (master)** controller so Status can show load without instrumenting individual application features. Slave CPU is out of scope for this change.

## ADDED Requirements

### Requirement: Host samples busy percent from FreeRTOS idle time
The host SHALL compute CPU busy percent from FreeRTOS run-time statistics as approximately 100 minus the idle-task share over a sampling window. On dual-core hosts the host SHALL report busy percent for CPU0 and CPU1 separately. The host SHALL NOT require per-feature application timers for this metric.

#### Scenario: Dual-core host reports two percents
- **WHEN** the host has completed at least one sampling window on a dual-core SoC
- **THEN** gateway status exposes distinct busy percents for CPU0 and CPU1

#### Scenario: Sample not ready
- **WHEN** Status is read before the first sampling window completes
- **THEN** host CPU fields are omitted or marked unavailable (not a fabricated 0% busy)

### Requirement: Percents are whole numbers in range
Reported busy percents SHALL be integers from 0 through 100 inclusive once a sample is available.

#### Scenario: Clamp to range
- **WHEN** a sampling window yields a computed busy value
- **THEN** the published percent is an integer between 0 and 100
