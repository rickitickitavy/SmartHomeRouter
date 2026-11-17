# Spec Delta

## Purpose

Lets multiple gateways form a STAR interconnect: one Main aggregates Zigbee traffic from paired Repeater hosts over short AEAD-protected TCP sessions so operators manage remote radios from a single console.

## ADDED Requirements

### Requirement: Network role is Main or Repeater
The host SHALL persist an interconnect network role of `Main` or `Repeater` and a router name of at most 32 characters. Role `Main` SHALL keep today’s single-gateway operator behavior for local Zigbee plus interconnect aggregation. Role `Repeater` SHALL forward Zigbee radio traffic to the paired Main and SHALL NOT run Place, Devices, Automation, Cameras, Scenes, Consoles, Security, or full Maintenance/Hardware operator workflows.

#### Scenario: Default role is Main
- **WHEN** interconnect settings have never been saved
- **THEN** network role is `Main`

#### Scenario: Name length enforced
- **WHEN** the operator saves a router name longer than 32 characters
- **THEN** the host rejects the save or truncates consistently so the stored name never exceeds 32 characters

### Requirement: Pairing port is configurable
The host SHALL persist a pairing TCP listen port. Factory default SHALL be **10241**. Saving the port SHALL take effect for the next pairing listen without requiring a full firmware reflash.

#### Scenario: Default pairing port
- **WHEN** the pairing port has never been saved
- **THEN** the stored pairing port is 10241

### Requirement: Short sessions use shared-secret AEAD
All interconnect exchanges (pairing after secret establishment, system-parameter sync, data exchange, checkAlive) SHALL use short TCP sessions: connect, exchange, acknowledge, disconnect. After pairing, payloads SHALL be protected with a shared secret and AEAD (authenticated encryption). The host SHALL NOT require TLS/SSL for these short sessions.

#### Scenario: Data exchange is a short session
- **WHEN** an initiator sends a Zigbee packet type message to a remote router
- **THEN** it opens TCP, completes the AEAD-protected exchange, receives OK, and disconnects

#### Scenario: No TLS required
- **WHEN** two paired routers exchange checkAlive or Zigbee packet data
- **THEN** the session succeeds without a TLS handshake

### Requirement: Pairing protocol establishes peer identity and secret
Repeater SHALL listen on the pairing port; Main SHALL connect and run the documented handshake and identity exchange, then both SHALL establish the shared AEAD secret. Main SHALL assign a unique non-zero 4-byte router id and persist the Repeater only after the protocol completes successfully.

#### Scenario: Successful pair adds repeater
- **WHEN** Main completes the full pairing protocol with a waiting Repeater
- **THEN** Main stores that Repeater’s name, MAC, IP, exchange port, router id, and shared secret material

#### Scenario: Incomplete pair does not persist
- **WHEN** pairing is cancelled or times out before both OKs
- **THEN** Main does not add a new paired-router row

#### Scenario: Handshake sequence
- **WHEN** Main connects to a waiting Repeater for pairing
- **THEN** both sides exchange the fixed handshake (`ImHere` / `copy`) before MAC/name and Main listen-port fields

### Requirement: Repeater pairing wait is timed and cancellable
On Repeater, starting **Pair to host** SHALL open the pairing port for at most **120** seconds, show a modal with countdown and **Cancel**, and blink the host RED status LED at **2 Hz**. The wait SHALL end on successful pairing, Cancel, or timeout, and SHALL then stop the LED blink and close the listen when no longer needed.

#### Scenario: Timeout ends wait
- **WHEN** Repeater pairing wait reaches 120 seconds with no successful pair
- **THEN** the modal closes, RED blink stops, and the pairing listen ends

#### Scenario: Cancel ends wait
- **WHEN** the operator presses Cancel during Repeater pairing wait
- **THEN** the wait ends immediately without completing pairing

### Requirement: Main pairing supports IP and subnet scan
On Main, **Pair** SHALL open a dialog with optional IP, port (default 10241), **Start**, and **Cancel**. When IP is filled, Main SHALL run the pairing protocol toward that address. When IP is empty, Main SHALL scan the local subnet (skipping already-paired Repeater IPs), attempt handshake-only discovery, and ask the operator to confirm each found router before running the full pairing protocol.

#### Scenario: Direct IP pair
- **WHEN** the operator enters an IP and port and presses Start
- **THEN** Main connects to that host and runs the full pairing protocol

#### Scenario: Scan finds and confirms
- **WHEN** IP is empty, Start is pressed, and a handshake-capable router is found
- **THEN** Main asks the operator whether that router is the intended peer before completing full pairing

#### Scenario: Operator declines found peer
- **WHEN** the operator answers no to a found peer during scan
- **THEN** Main continues scanning for the next candidate

### Requirement: Main shows paired routers table
When role is Main, Interconnection SHALL show a table of paired routers with Name, MAC, IP, Port, online indicator (green or gray), device count, packets In, and packets Out, plus a **Pair** control below the table.

#### Scenario: Online indicator
- **WHEN** a paired Repeater answered the latest checkAlive within the configured interval window
- **THEN** its online indicator is green; otherwise it is gray

### Requirement: Repeater shows Main peer status
When role is Repeater and a Main has been paired, Interconnection SHALL show that Main’s IP, name, MAC, and online indicator (green or gray).

#### Scenario: Main online on Repeater
- **WHEN** the Repeater has received Main traffic or checkAlive within the expected interval
- **THEN** the Main online indicator is green

### Requirement: CheckAlive interval is Main-owned and shared
Main SHALL persist checkAlive Repeaters interval from **5** to **600** seconds (default **60**). Changing it on Main SHALL push the new value to all paired Repeaters via the system-parameters exchange. After Main reboot, Main SHALL share checkAlive interval and zigbee search interval to all paired Repeaters again.

#### Scenario: Interval change pushes to repeaters
- **WHEN** an admin saves checkAlive interval 30 on Main with two paired Repeaters
- **THEN** both Repeaters store 30 and use it to judge Main liveness

#### Scenario: Out of range rejected or clamped
- **WHEN** the operator submits checkAlive interval 3 or 900
- **THEN** the host rejects the value or clamps it into 5–600 consistently

### Requirement: Zigbee search interval is Main-owned and shared
Main SHALL own a zigbee search interval (search/permit-join window duration) shared to all Repeaters via system-parameters exchange on change and after Main reboot. Repeaters SHALL apply the received value for interconnect-driven search.

#### Scenario: Search interval shared after reboot
- **WHEN** Main reboots with paired Repeaters
- **THEN** Main pushes zigbee search interval to each Repeater again

### Requirement: System parameters use short AEAD sessions
System-parameter exchange SHALL follow: Repeater listens; Main connects; handshake; Main sends `variable=value`; Repeater saves and replies OK; Main disconnects. Protected with the paired shared-secret AEAD after pairing.

#### Scenario: Parameter applied on Repeater
- **WHEN** Main sends checkAlive interval as a system parameter
- **THEN** Repeater persists that value and acknowledges OK

### Requirement: Data exchange carries typed short messages
Data exchange SHALL send a 1-byte type (packet, setting, or event) then the payload over a short AEAD-protected TCP session; the remote SHALL reply OK; the initiator SHALL disconnect. Main SHALL verify the MAC of a connecting Repeater against the paired table before accepting the session.

#### Scenario: Unknown MAC rejected
- **WHEN** a host connects to Main’s exchange port with a MAC not in the paired table
- **THEN** Main rejects the session without applying the payload

### Requirement: Repeater forwards Zigbee packets to Main
A Repeater SHALL forward every Zigbee packet it receives from its local radio to Main using data exchange, except duplicates it already forwarded.

#### Scenario: Non-duplicate report forwarded
- **WHEN** the Repeater receives a new Zigbee attribute report from a local device
- **THEN** it sends that packet to Main with type packet

#### Scenario: Duplicate suppressed
- **WHEN** the Repeater sees the same Zigbee packet again as a duplicate
- **THEN** it does not forward that duplicate to Main

### Requirement: Main injects remote packets into local Zigbee ingest
When Main receives a Zigbee packet from a Repeater, it SHALL feed that packet into the same host ingest path used for local slave Zigbee events so automations, live UI, and MQTT behave as if the packet arrived from the local radio.

#### Scenario: Remote report reaches ingest
- **WHEN** Main receives a packet-type message from a paired Repeater
- **THEN** the host status/live pipeline updates as for an equivalent local SPI Zigbee event

### Requirement: Main routes outbound Zigbee by router id
When Main must send a Zigbee command to a registered device, it SHALL use that device’s `router_id`. Value `0` SHALL mean the local radio. A non-zero id SHALL send the command to the matching paired Repeater via data exchange; that Repeater SHALL transmit on its local radio.

#### Scenario: Local device stays on SPI
- **WHEN** Main commands a device whose `router_id` is 0
- **THEN** the command is sent on the local host→slave Zigbee path

#### Scenario: Remote device uses interconnect
- **WHEN** Main commands a device whose `router_id` matches a paired Repeater
- **THEN** Main sends the command to that Repeater over data exchange and does not enqueue it as a local SPI target for that IEEE

### Requirement: Multi-router search aggregates found devices
When Main starts device search, it SHALL open search on the local radio and command all paired Repeaters to search. Found devices from Repeaters SHALL appear in Main’s found list with the source router identity. Registering a found remote device SHALL store that router’s id on the device record.

#### Scenario: Remote found shows router
- **WHEN** a Repeater finds an unregistered IEEE during Main-driven search
- **THEN** Main’s found table includes that IEEE and identifies the source Repeater

### Requirement: Interconnect settings export with global settings
Global settings export and restore SHALL include interconnect configuration (role, router name, pairing port, checkAlive interval, zigbee search interval as owned by this host, paired-router table fields needed to restore peering, excluding Wi‑Fi credentials). Shared secrets SHALL be included only in a form that restores peering without exposing Wi‑Fi passwords.

#### Scenario: Export includes interconnect
- **WHEN** an admin exports settings on a Main with one paired Repeater
- **THEN** the JSON includes interconnect settings and that paired router’s persisted identity fields

#### Scenario: Restore reapplies paired table
- **WHEN** an admin restores a file that contains interconnect paired routers
- **THEN** the host replaces interconnect settings from that slice when present
