# Spec Delta

## Purpose

Provides a bounded host PSRAM FIFO that paces automation action execution and, when enabled, base UI device commands, so bursts are drained from the main loop instead of applied inline on trigger or HTTP paths.

## ADDED Requirements

### Requirement: Action and command FIFO capacity

The host MUST provide one FIFO queue for deferred work with a maximum of 1024 elements. Queue storage MUST use PSRAM when available. Static allocation of the queue backing store MUST be allowed.

#### Scenario: Capacity is 1024

- **WHEN** the host starts with PSRAM available
- **THEN** the action/command FIFO accepts at most 1024 pending elements

### Requirement: Overflow ignores new elements

When the FIFO is full, the host MUST ignore (drop) new enqueue attempts and MUST NOT replace or remove already queued elements to make room.

#### Scenario: Full queue drops newest

- **WHEN** the FIFO already holds 1024 elements and another enqueue is attempted
- **THEN** the new element is not queued and the existing 1024 elements remain unchanged

### Requirement: Automation actions always enqueue

Whenever an automation action must execute (device, automation-control, or e-mail action application), the host MUST place that work on the FIFO instead of applying it inline on the trigger, timer, nested-run, or console run-action path. Nested or multi-action lists MUST enqueue each action in list order.

#### Scenario: Trigger enqueues rather than inline apply

- **WHEN** a trigger would run two device actions
- **THEN** both actions are placed on the FIFO in order and neither device command is applied before the main-loop drain executes them

#### Scenario: Console run-action enqueues

- **WHEN** an authenticated client requests Run for an automation with three actions
- **THEN** those three actions are enqueued in order on the same FIFO

### Requirement: Main-loop FIFO drain with minimum spacing

The host MUST execute queued work FIFO from the main loop. After successfully starting one element’s execution, the host MUST wait at least 100 milliseconds before starting the next element.

#### Scenario: Spacing between two items

- **WHEN** two device actions are queued and the main loop drains the queue
- **THEN** the second action’s execution starts no earlier than 100 ms after the first action’s execution started

#### Scenario: Empty queue is idle

- **WHEN** the FIFO is empty
- **THEN** the main loop does not invent work and the busy count is zero

### Requirement: Hardware flag gates base UI device commands

The host MUST persist a boolean **use queue for base device command**, default false. When the flag is false, base UI device commands MUST execute on the existing immediate command path. When the flag is true, base UI device commands for all device kinds MUST enqueue on the same FIFO used for automation actions. Devices Configure dialog attribute writes MUST NOT use this FIFO regardless of the flag.

#### Scenario: Flag off leaves UI command immediate

- **WHEN** the flag is false and the operator sends ON via Manual command or a console On/Off widget
- **THEN** the command is applied on the immediate device-command path and is not placed on the action/command FIFO

#### Scenario: Flag on queues UI command for all kinds

- **WHEN** the flag is true and the operator sends a covering OPEN, an On/Off OFF, or a custom/cluster body via the base device-command API
- **THEN** each such command is enqueued on the FIFO and drained with the same ≥100 ms spacing

#### Scenario: Configure excluded

- **WHEN** the operator writes a configure-catalog attribute through the Devices Configure path
- **THEN** that write does not enter the action/command FIFO

### Requirement: Status exposes queue busy count

The gateway status payload MUST include the current number of pending elements in the action/command FIFO (busy count). Operator and simple-user Status Free memory groups MUST show that busy count.

#### Scenario: Busy count matches pending

- **WHEN** five elements are pending in the FIFO
- **THEN** status reports busy count 5 and Free memory shows that value on both operator and simple-user Status
