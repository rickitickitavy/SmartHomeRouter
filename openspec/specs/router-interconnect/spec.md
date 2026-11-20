# router-interconnect Specification

## Purpose

Lets multiple gateways form a STAR interconnect: one Main aggregates Zigbee traffic from paired Repeater hosts over short AEAD-protected TCP sessions so operators manage remote radios from a single console.

## Requirements

### Requirement: Network role is Main or Repeater
The host SHALL persist an interconnect network role of `Main` or `Repeater` and a router name of at most 32 characters. Role `Main` SHALL keep today’s single-gateway operator behavior for local Zigbee plus interconnect aggregation. Role `Repeater` SHALL forward Zigbee radio traffic to the paired Main and SHALL NOT run Place, Devices, Automation, Cameras, Scenes, Consoles, Security, or full Maintenance/Hardware operator workflows.

#### Scenario: Default role is Main
- **WHEN** interconnect settings have never been saved
- **THEN** network role is `Main`

#### Scenario: Name length enforced
- **WHEN** the operator saves a router name longer than 32 characters
- **THEN** the host rejects the save or truncates consistently so the stored name never exceeds 32 characters

### Requirement: Role change clears peers after double confirmation
Changing network role from Main to Repeater or Repeater to Main SHALL require two operator confirmations. After the second confirm, the host SHALL clear the paired-router table and peer secrets (including a Repeater’s stored Main peer), then SHALL reboot to apply the new role.

#### Scenario: First confirm alone does not clear
- **WHEN** the operator changes role and accepts only the first confirm
- **THEN** the paired table is unchanged

#### Scenario: Second confirm clears peers and reboots
- **WHEN** the operator completes both confirms for a role change
- **THEN** all paired peer records and secrets are removed and the host schedules a restart

### Requirement: One TCP port for pairing and exchange
The host SHALL persist a single interconnect TCP port used for pairing and for all post-pair universal exchanges. Factory default SHALL be **10241**.

#### Scenario: Default port
- **WHEN** the interconnect port has never been saved
- **THEN** the stored port is 10241

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

### Requirement: Only pairing and universal exchange protocols
Interconnect SHALL use exactly two protocols: the pairing protocol, and the universal exchange protocol. The host SHALL NOT require TLS/SSL for either.

#### Scenario: Post-pair traffic uses universal exchange
- **WHEN** a paired Repeater forwards a Zigbee packet or Main sends checkAlive
- **THEN** the session follows the universal exchange sequence, not a separate ad-hoc protocol

### Requirement: Pairing protocol establishes peer identity and secret
Repeater SHALL listen on the interconnect port; Main SHALL connect and run the documented handshake and identity exchange, then both SHALL establish the shared AEAD secret. Main SHALL assign a unique non-zero 4-byte router id and persist the Repeater only after the protocol completes successfully.

#### Scenario: Successful pair adds repeater
- **WHEN** Main completes the full pairing protocol with a waiting Repeater
- **THEN** Main stores that Repeater’s name, MAC, IP, port, router id, and shared secret material

#### Scenario: Incomplete pair does not persist
- **WHEN** pairing is cancelled or times out before both OKs
- **THEN** Main does not add a new paired-router row

#### Scenario: Handshake sequence
- **WHEN** Main connects to a waiting Repeater for pairing
- **THEN** both sides exchange the fixed handshake (`ImHere` / `copy`) before MAC/name and Main port/secret fields

### Requirement: Universal exchange sequence
A universal exchange SHALL be: TCP connect to the peer interconnect port; handshake `ImHere` / `copy`; a versioned header (`version`, `type`, `len`, nonce) then AEAD ciphertext of the payload at most **256** bytes with AAD bound to the peer MAC; receiver replies **`OK`**; initiator disconnects. Either paired peer MAY initiate. The receiver SHALL NOT send a typed reply body. v1 `version` SHALL be 1.

#### Scenario: Always OK
- **WHEN** a valid AEAD exchange message is accepted
- **THEN** the receiver replies with OK and no typed payload

#### Scenario: Bad AEAD rejected
- **WHEN** the AEAD tag is invalid
- **THEN** the receiver does not apply the payload as a successful exchange

#### Scenario: Oversize payload rejected
- **WHEN** an exchange payload length exceeds 256 bytes
- **THEN** the receiver rejects it without applying it as success

#### Scenario: Unknown version rejected
- **WHEN** the header version is not 1
- **THEN** the receiver rejects the session without applying the payload

### Requirement: Exchange type enum
Universal exchange `type` SHALL be one of: `just_booted` (1), `variable` (2), `check_alive` (3), `zigbee_packet` (4), `search_start` (5), `search_stop` (6), `rebind` (7), `rebind_result` (8). Value 0 is reserved. The host SHALL ignore or reject unknown types without applying them as success.

#### Scenario: Known type accepted
- **WHEN** a paired peer sends type `check_alive` with a valid AEAD blob
- **THEN** the receiver handles checkAlive and replies OK

### Requirement: Rebind exchanges for remote devices
For non-zero `router_id`, Main SHALL send `rebind` to that Repeater; the Repeater SHALL run local Zigbee rebind and return `rebind_result`. Main SHALL wait up to **30** seconds (same wait for local `router_id` 0 SPI rebind). Outcomes remain success, pairing fallback, or failure; the registry record SHALL stay.

#### Scenario: Remote rebind success
- **WHEN** the operator rebinds a Zigbee device on a paired Repeater and local immediate bind succeeds
- **THEN** Main receives rebind_result success and the operator API reports immediate success

#### Scenario: Remote rebind targets only owning router
- **WHEN** the operator rebinds a registered device whose `router_id` matches one paired Repeater
- **THEN** Main sends `rebind` only to that Repeater and does not open search/rebind on other interconnect peers or the local radio

#### Scenario: Remote rebind fallback
- **WHEN** the Repeater falls back to pairing-window rebind for that IEEE
- **THEN** Main receives rebind_result pairing fallback and keeps the registered record

#### Scenario: Rebind timeout
- **WHEN** no rebind_result arrives within 30 seconds
- **THEN** Main reports rebind failure and keeps the registered record

### Requirement: Re-pair same Repeater by stored IP then MAC scan
Main SHALL offer **Re pair** for a paired Repeater. Re-pair SHALL first attempt pairing to the stored IP/port and SHALL require the answering host MAC to match the stored MAC. If that address has no matching Repeater, Main SHALL scan the subnet for the stored MAC and pair automatically when found. If neither path succeeds, re-pair SHALL fail and the peer row SHALL remain until the operator deletes it.

#### Scenario: Stored IP answers with matching MAC
- **WHEN** Re pair is started and the stored IP answers with the stored MAC while Pair to host is active
- **THEN** pairing completes and the same router id is kept

#### Scenario: Stored IP missing falls back to MAC scan
- **WHEN** Re pair cannot reach a matching Repeater at the stored IP
- **THEN** Main scans the subnet for the stored MAC and pairs that host, or reports re-pair failed if not found

### Requirement: Re-pair with new keeps router id and device binds
Main SHALL offer **Re-pair with new** on a paired Repeater card. That flow SHALL use the same Pair sequence (explicit IP or subnet scan with operator confirm). On success Main SHALL replace the peer’s MAC/IP/secret/name while keeping the same `router_id` and Zigbee channel so registered device binds to that router id stay valid.

#### Scenario: Replacement hardware keeps binds
- **WHEN** the operator completes Re-pair with new against a different MAC while Pair to host is active
- **THEN** the peer row shows the new MAC/IP and devices that stored that router id remain bound to it

### Requirement: Main verifies connecting Repeater identity
When a host connects to Main for universal exchange, Main SHALL accept the session only if the peer matches a paired Repeater (MAC/secret established at pairing).

#### Scenario: Unknown peer rejected
- **WHEN** an unpaired host connects to Main’s interconnect port for exchange
- **THEN** Main rejects the session without applying the payload

### Requirement: Repeater pairing wait is timed and cancellable
On Repeater, starting **Pair to host** SHALL listen on the interconnect port for at most **120** seconds, show a modal with countdown and **Cancel**, and blink the host RED status LED at **2 Hz**. The wait SHALL end on successful pairing, Cancel, or timeout.

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

### Requirement: Repeater shows Main peer status
When role is Repeater and a Main has been paired, Interconnection SHALL show that Main’s IP, name, MAC, and online indicator (green or gray).

#### Scenario: Main online on Repeater
- **WHEN** the Repeater has received Main traffic or checkAlive within the expected interval
- **THEN** the Main online indicator is green

### Requirement: CheckAlive is Main to Repeater only
Main SHALL persist checkAlive interval from **5** to **600** seconds (default **60**) and SHALL send `check_alive` exchanges to each paired Repeater on that interval. Changing the interval on Main SHALL push it to all Repeaters via `variable` exchanges.

#### Scenario: Interval change pushes to repeaters
- **WHEN** an admin saves checkAlive interval 30 on Main with two paired Repeaters
- **THEN** both Repeaters store 30 and use it to judge Main liveness

#### Scenario: Out of range rejected or clamped
- **WHEN** the operator submits checkAlive interval 3 or 900
- **THEN** the host rejects the value or clamps it into 5–600 consistently

### Requirement: Zigbee search interval is Main-owned and shared
Main SHALL own zigbee search interval (search/permit-join window duration) and SHALL share it to Repeaters via `variable` on change and after Main reboot. Repeaters SHALL apply it for interconnect-driven search.

#### Scenario: Search interval shared after Main reboot
- **WHEN** Main reboots with paired Repeaters
- **THEN** Main pushes zigbee search interval to each Repeater again via variable exchanges

### Requirement: Zigbee channels are unique and Main-assigned
Every router in the STAR SHALL use a different Zigbee channel. Main SHALL assign each Repeater a channel from **11–26** that is not used by Main or by any other paired Repeater. Main SHALL set the Repeater channel at pairing, on that Repeater’s `just_booted`, and whenever Main recalculates assignments, via `variable`. The Repeater SHALL apply the received channel locally. Main SHALL refuse pairing if no free channel remains.

#### Scenario: Pairing assigns distinct channel
- **WHEN** Main completes pairing with a Repeater while Main uses channel 15 and another Repeater uses 20
- **THEN** the new Repeater receives a channel other than 15 and 20 in 11–26

#### Scenario: Boot refreshes channel
- **WHEN** a paired Repeater sends just_booted
- **THEN** Main includes that Repeater’s assigned Zigbee channel in the following variable pushes

#### Scenario: No free channel blocks pair
- **WHEN** all channels 11–26 that Main would use for a new peer are already taken by Main and existing Repeaters
- **THEN** Main does not complete pairing for an additional Repeater

### Requirement: Repeater boot hello then Main variable push
When a paired Repeater becomes ready after boot, it SHALL send a `just_booted` exchange including its host firmware version to Main. Main SHALL then push all Main-owned shared settings to that Repeater using separate `variable` exchanges (not inside the hello session), including checkAlive interval, zigbee search interval, and that Repeater’s Zigbee channel.

#### Scenario: Boot triggers settings refresh
- **WHEN** Repeater completes just_booted to Main
- **THEN** Main sends checkAlive interval, zigbee search interval, and Zigbee channel as variable exchanges and each is acknowledged with OK

### Requirement: Zigbee packet uses SPI-equivalent payload
A `zigbee_packet` payload SHALL use the same event format the host already parses from SPI Zigbee events. Main SHALL inject a received `zigbee_packet` into the same ingest path as a local SPI event. A Repeater SHALL transmit a received command `zigbee_packet` on its local radio.

#### Scenario: Remote join reaches found path
- **WHEN** Repeater forwards a join-shaped zigbee_packet during search and the IEEE is not registered on Main
- **THEN** Main’s found list includes that IEEE with that Repeater’s router id

#### Scenario: Local SPI and socket same parser
- **WHEN** Main receives a zigbee_packet whose bytes match a local SpiEvtAttrReport layout
- **THEN** the host status/live pipeline updates as for the equivalent local SPI event

### Requirement: Repeater forwards all non-duplicate Zigbee RX
A Repeater SHALL forward every Zigbee packet from its local radio to Main as `zigbee_packet`, except duplicates. A duplicate SHALL mean the same IEEE, same SPI command id, and identical payload bytes as a packet already forwarded within a short rolling recent window. The Repeater SHALL NOT require a known-device list from Main to filter forwards.

#### Scenario: Non-duplicate report forwarded
- **WHEN** the Repeater receives a new Zigbee attribute report from a local device
- **THEN** it sends zigbee_packet to Main

#### Scenario: Duplicate suppressed
- **WHEN** the Repeater sees the same IEEE, SPI command, and payload bytes again within the recent window
- **THEN** it does not forward that duplicate to Main

### Requirement: Main routes outbound Zigbee by router id
When Main must send a Zigbee command to a registered device, it SHALL use that device’s `router_id`. Value `0` SHALL mean the local radio. A non-zero id SHALL send a `zigbee_packet` to the matching paired Repeater.

#### Scenario: Local device stays on SPI
- **WHEN** Main commands a device whose `router_id` is 0
- **THEN** the command is sent on the local host→slave Zigbee path

#### Scenario: Remote device uses interconnect
- **WHEN** Main commands a device whose `router_id` matches a paired Repeater
- **THEN** Main sends zigbee_packet to that Repeater and does not enqueue it as a local SPI target for that IEEE

### Requirement: Multi-router search uses search_start and forwarded packets
When Main starts device search, it SHALL open search on the local radio and send `search_start` to all paired Repeaters. Found devices SHALL appear from local joins and from forwarded `zigbee_packet` joins; Main SHALL stamp the source router id. Main MAY send `search_stop` to Repeaters when search ends early.

#### Scenario: Remote found via forwarded join
- **WHEN** a Repeater’s radio joins an unregistered IEEE during Main-driven search and forwards zigbee_packet
- **THEN** Main’s found table includes that IEEE and identifies the source Repeater

### Requirement: Interconnect settings export with global settings
Global settings export and restore SHALL include interconnect configuration (role, router name, port, checkAlive interval, zigbee search interval as owned by this host, paired-router identity fields needed to restore peering, excluding Wi‑Fi credentials). Restore SHALL fully replace the paired-router table from the interconnect slice when that slice is present.

#### Scenario: Export includes interconnect
- **WHEN** an admin exports settings on a Main with one paired Repeater
- **THEN** the JSON includes interconnect settings and that paired router’s persisted identity fields

#### Scenario: Restore reapplies paired table
- **WHEN** an admin restores a file that contains interconnect paired routers different from the current table
- **THEN** the host fully replaces the paired table from that slice (peers absent from the file are removed)

### Requirement: Offline peer affects device command UX
When a device’s `router_id` refers to a paired Repeater that is offline, Main SHALL present that device as interconnect-offline (distinct from local radio unknown) and SHALL NOT send Zigbee commands to it as if the Repeater were reachable; the operator-facing result SHALL state the router is offline. Local `router_id` 0 devices SHALL keep today’s online rules.

#### Scenario: Command blocked when repeater offline
- **WHEN** the operator commands a device whose Repeater online indicator is gray
- **THEN** the command is not sent over interconnect and the UI/API indicates the router is offline

### Requirement: Orphan device router_id can be reassigned from Main
On Main, when a registered Zigbee device’s `router_id` refers to a Repeater that is no longer paired, the operator MUST be able to change that device’s `router_id` to **local** (`0`) or to a currently paired Repeater’s id through the device dialog Save path, without deleting the device. After Save, Zigbee command routing MUST use the new `router_id` (local radio for `0`, or the matching peer for a non-zero id) under existing interconnect command rules.

#### Scenario: Reassign orphan to local
- **WHEN** a registered device has `router_id` 4, peer 4 is unpaired, and the operator Saves with local selected
- **THEN** the device’s stored `router_id` is 0 and later commands use the local radio path

#### Scenario: Reassign orphan to another paired Repeater
- **WHEN** a registered device is orphaned from `router_id` 4 and the operator Saves with paired Repeater id 2 selected
- **THEN** the device’s stored `router_id` is 2 and later commands target peer 2 when that peer is online

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

### Requirement: Main pushes time zone offset to Repeaters
When network role is Main and the operator saves a UTC time-zone offset (minutes from UTC, 30-minute steps), the host SHALL push that offset to all paired Repeaters via the interconnect variable exchange (same Main-owned push path as other shared settings). A Repeater SHALL apply the received offset to its local wall-clock / NTP configuration and SHALL NOT treat a conflicting local Hardware edit as authoritative.

#### Scenario: Save on Main updates paired Repeaters
- **WHEN** Main saves timeZoneUtcOffsetMin 180 with two online paired Repeaters
- **THEN** both Repeaters receive and apply offset +180 minutes

#### Scenario: Boot hello refreshes offset
- **WHEN** a paired Repeater completes just_booted with Main
- **THEN** Main includes the current UTC offset in the shared variable push to that peer

### Requirement: Status API exposes aggregated repeater packet totals
On Main, the gateway status (or interconnect status slice used by Status) SHALL expose aggregated **Repeater packets sent** and **Repeater packets received** as the sum of paired peers’ interconnect packets Out and packets In respectively. When role is Repeater or there are no peers, both aggregates SHALL be 0.

#### Scenario: Aggregate from peers
- **WHEN** Main has peers with packetsOut 10 and 5 and packetsIn 3 and 2
- **THEN** status reports repeater packets sent 15 and received 5 for Status Network
