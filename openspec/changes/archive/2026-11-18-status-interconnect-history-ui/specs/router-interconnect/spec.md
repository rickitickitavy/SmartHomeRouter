# Spec Delta: router-interconnect

## ADDED Requirements

### Requirement: Paired Repeater device count reflects bound devices
On Main, each paired Repeater’s live **device** count SHALL equal the number of registered Zigbee devices whose `router_id` matches that peer’s router id. The count SHALL update when devices are added, removed, or rebound so Status and the Interconnection paired table do not stay at zero while remote devices exist.

#### Scenario: Remote devices counted
- **WHEN** Main has a paired Repeater with router id 3 and three registered devices with `router_id` 3
- **THEN** that Repeater’s device count is 3 on the paired table and in interconnect status payloads

#### Scenario: Local devices excluded
- **WHEN** registered devices use `router_id` 0 and one Repeater is paired
- **THEN** that Repeater’s device count does not include those local devices

### Requirement: Status Interconnection exposes queue and peer versions
The gateway status / interconnect live payload used by Status SHALL expose interconnect queue used and depth and each paired peer’s last known host VERSION so the Status Interconnection widget can render them. Queue used/depth NEED NOT be the operator-facing placement on the Interconnection settings tab.

#### Scenario: Status can show queue
- **WHEN** Status polls while the interconnect queue holds 0 of 1024 jobs
- **THEN** the status/interconnect payload provides used 0 and depth 1024 for Status

#### Scenario: Peer version available
- **WHEN** a paired Repeater has reported host version during checkAlive or hello
- **THEN** the peer live fields include that host version for Status

### Requirement: Interconnect import fully overrides paired table
Maintenance Interconnection import and the interconnect slice of global settings restore SHALL fully replace the paired-router table from the file (no MAC-authoritative keep). Absent peers are removed; new peers are added within the seven-Repeater cap; channel conflicts trigger reassignment.

#### Scenario: Import fully replaces peers
- **WHEN** Main has two paired Repeaters and an import lists only one different MAC set
- **THEN** after import the paired table matches the file (within cap) and the previous two peers are not kept solely because their MACs were already local

#### Scenario: Import adds within cap
- **WHEN** Main has one paired Repeater and an import lists that peer plus additional peers within the seven-Repeater limit
- **THEN** the resulting table matches the import set

## MODIFIED Requirements

### Requirement: Request queue windows interconnect sessions
Because one port serves all exchange traffic and either peer may initiate, the host SHALL queue interconnect jobs in a PSRAM queue of depth **1024**. When the queue is full, the host SHALL reject new jobs (not enqueue them). Pair-to-host SHALL NOT freeze other interconnect sessions; the listener SHALL demultiplex pairing versus universal exchange after the shared handshake.

#### Scenario: Burst is queued
- **WHEN** multiple Zigbee forwards are ready while a session is in flight
- **THEN** later work waits in the queue rather than opening unbounded parallel sessions

#### Scenario: Full queue rejects new
- **WHEN** the interconnect queue already holds 1024 jobs and another job is submitted
- **THEN** the new job is rejected and not enqueued

#### Scenario: Exchange during pairing wait
- **WHEN** Pair-to-host listen is active and a valid universal-exchange session arrives
- **THEN** the host handles that exchange without requiring pairing to finish first

### Requirement: Main shows paired routers table
When role is Main, Interconnection SHALL show a table of paired routers with Name, MAC, IP, Port, online indicator (green or gray), device count (devices bound to that router id), packets In, and packets Out, plus a **Pair** control below the table. The STAR SHALL allow at most **8** routers total (1 Main + at most **7** paired Repeaters). While the Interconnection section is open, the host SHALL push peer live fields (online, packets In/Out, device count, and related status) over WebSocket `/ws/interconnect` so the table refreshes without a manual Reload.

#### Scenario: Online indicator
- **WHEN** a paired Repeater answered the latest checkAlive within the configured interval window
- **THEN** its online indicator is green; otherwise it is gray

#### Scenario: Live table refresh on packet counters
- **WHEN** Interconnection is open on Main and a peer’s packets In or Out changes
- **THEN** the Repeaters table updates those counts via `/ws/interconnect` without requiring Reset/Save

#### Scenario: Eighth repeater rejected
- **WHEN** Main already has 7 paired Repeaters and pairing would add another
- **THEN** Main refuses the new peer without dropping existing ones

#### Scenario: Device count non-zero
- **WHEN** Interconnection is open on Main and a Repeater has two bound devices
- **THEN** that row’s device count shows 2

### Requirement: Interconnect settings export with global settings
Global settings export and restore SHALL include interconnect configuration (role, router name, port, checkAlive interval, zigbee search interval as owned by this host, paired-router identity fields needed to restore peering, excluding Wi‑Fi credentials). Restore SHALL fully replace the paired-router table from the interconnect slice when that slice is present.

#### Scenario: Export includes interconnect
- **WHEN** an admin exports settings on a Main with one paired Repeater
- **THEN** the JSON includes interconnect settings and that paired router’s persisted identity fields

#### Scenario: Restore reapplies paired table
- **WHEN** an admin restores a file that contains interconnect paired routers different from the current table
- **THEN** the host fully replaces the paired table from that slice (peers absent from the file are removed)

## REMOVED Requirements

### Requirement: Interconnect import keeps existing peers authoritative
**Reason**: Operators need Export/Import and settings restore to replace the paired Repeater table from the file, not keep existing MAC matches authoritative.
**Migration**: Use full-override import (`keepExistingMacAuthoritative` false / replace-from-file). Re-export after pairing if a keep-existing merge was previously relied on.
