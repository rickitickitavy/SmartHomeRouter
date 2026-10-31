# zigbee-slave-radio Specification

## Purpose

Keeps the Zigbee coordinator on the slave independent of host Wi-Fi mode, after the host has pushed settings for that boot.

## Requirements

### Requirement: Zigbee independent of Wi-Fi mode
After the host has accepted a settings-applied result for the current slave boot, the slave SHALL keep the Zigbee coordinator running while powered and not in a fatal radio error, including while the host is in SoftAP, STA, or recovery AP. Until that settings apply, the slave SHALL run SPI and the RAM log only. The host SHALL NOT start a Zigbee radio on its own chip.

#### Scenario: Host in SoftAP
- **WHEN** the host is serving SoftAP (including recovery AP) and slave settings have been applied
- **THEN** the slave Zigbee coordinator remains up and continues to receive and send

#### Scenario: Host STA connected
- **WHEN** the host is joined to a router and slave settings have been applied
- **THEN** the slave Zigbee coordinator remains up and continues to receive and send

#### Scenario: Before settings
- **WHEN** the slave has booted but has not yet applied host `SET_SETTINGS`
- **THEN** it does not start the coordinator and only runs SPI plus the RAM log

### Requirement: Host Wi-Fi stays usable
With the two-chip split, the host SHALL run Wi-Fi (SoftAP or STA) without enabling IEEE 802.15.4 coexist on the host radio. Operator-visible services on the host (ping, HTTP web console, MQTT client) SHALL remain usable while the slave Zigbee radio is active.

#### Scenario: Web while mesh is up
- **WHEN** the slave coordinator is started and the host has a usable Wi-Fi interface
- **THEN** the host web console and ICMP to the host address remain reachable without requiring Zigbee to be stopped

### Requirement: One in-flight command per destination
The slave SHALL transmit at most one Zigbee on/off or write-attribute command at a time to a given IEEE and destination endpoint. While that command is in flight, a newer on/off or write-attribute for the same IEEE and endpoint SHALL replace the pending next command and SHALL NOT be put on the air until the in-flight command finishes or a send timeout expires. After finish or timeout, the slave SHALL send only that latest pending command, or none if none remains. Distinct endpoints of the same IEEE MAY have independent in-flight commands. Permit-join and other non-device-control radio work SHALL NOT be serialized behind this rule.

#### Scenario: Rapid ON then OFF
- **WHEN** the operator sends ON then OFF to the same IEEE and endpoint faster than the first command completes on air
- **THEN** the device receives at most the in-flight command plus a later OFF, and does not keep switching after OFF has been applied and in-flight work has finished

#### Scenario: Echo fades to last command
- **WHEN** ON and OFF are repeated several times to one endpoint and then commands stop
- **THEN** the device settles on the last commanded state and does not continue random ON/OFF from leftover earlier commands

#### Scenario: Two endpoints stay independent
- **WHEN** endpoint 1 is commanded OFF and endpoint 3 of the same IEEE is commanded ON before endpoint 1 completes
- **THEN** both destinations still receive their own commands

### Requirement: Classify device type from in-clusters
When the slave identifies a joining or newly bound device that is **not** already registered, it SHALL assign device type from the Zigbee **Application Device ID** in the ZDO Simple Descriptor response (Simple Descriptor Request `0x0004` / response `0x8004`) for a usable endpoint. Classification SHALL map that Device ID using the Home Automation / ZHA (and overlapping ZLO) Device ID table to the gateway’s stored type ids. In-clusters and out-clusters SHALL NOT be used to assign or merge device type. Manufacturer name, model, and friendly name SHALL NOT decide the type. If the Application Device ID is missing or not mapped, the type SHALL be `unknown`. The slave SHALL classify before or with the join report that the host uses for pairing search. For an IEEE already present in the registered device map, the slave SHALL keep the stored type (apply saved class) and SHALL NOT replace it from a new Simple Descriptor.

#### Scenario: Smart plug Device ID
- **WHEN** a new (unregistered) joining device’s Simple Descriptor reports Application Device ID `0x0051` (Smart Plug)
- **THEN** the slave classifies that IEEE as `onOff`

#### Scenario: On/Off light Device ID
- **WHEN** a new joining device’s Simple Descriptor reports Application Device ID `0x0100` (On/Off Light)
- **THEN** the slave classifies that IEEE as `onOff`

#### Scenario: Temperature sensor Device ID
- **WHEN** a new joining device’s Simple Descriptor reports Application Device ID `0x0302` (Temperature Sensor)
- **THEN** the slave classifies that IEEE with the temperature-measurement device type used by the gateway catalog

#### Scenario: IAS Zone Device ID
- **WHEN** a new joining device’s Simple Descriptor reports Application Device ID `0x0402` (IAS Zone)
- **THEN** the slave classifies that IEEE as `iasZone`

#### Scenario: Window covering Device ID
- **WHEN** a new joining device’s Simple Descriptor reports Application Device ID `0x0202` (Window Covering Device)
- **THEN** the slave classifies that IEEE as `windowCovering`

#### Scenario: Clusters alone do not set type
- **WHEN** a new joining device advertises in-cluster `0x0006` but Application Device ID is unmapped or absent
- **THEN** the slave classifies that IEEE as `unknown` (clusters MUST NOT fill type)

#### Scenario: Registered type is frozen
- **WHEN** an IEEE is already registered as `onOff` and a later Simple Descriptor would map to another type
- **THEN** the slave keeps type `onOff`

### Requirement: Application Device ID map covers HA and ZLO ids
The slave SHALL map at least these Application Device IDs to gateway types for new devices: On/Off family (`0x0000`, `0x0002`, `0x0009`, `0x0051`, `0x0100`, `0x0103`, `0x010A`, and other On/Off-equivalent HA/ZLO ids used by the product) → `onOff`; dimmable / level / color light and switch ids that the product treats as level or color classes → the corresponding catalog types; shade / window covering ids (`0x0200`–`0x0203`) → `windowCovering`; thermostat `0x0301` → thermostat catalog type; temperature sensor `0x0302` → temperature-measurement catalog type; IAS Zone `0x0402` → `iasZone`. Unlisted Device IDs SHALL map to `unknown` until explicitly added.

#### Scenario: Unlisted Device ID
- **WHEN** Application Device ID is `0x0008` (Range Extender) and no product mapping exists
- **THEN** type is `unknown`

### Requirement: Coordinator start reads current device status
After the slave Zigbee coordinator has started with host settings, and the registered Zigbee devices for that boot are on the slave, the slave SHALL request current status from every registered device it knows. For `onOff`, it SHALL read On/Off (`0x0006` / `0x0000`) on each usable channel endpoint for that device, including every endpoint from 1 through `channels` when `channels` is 2–16. Each answer SHALL be reported with the endpoint that sent it and SHALL NOT be applied to a different channel. Requesting several endpoints of the same device in that boot pass SHALL still obtain an answer for each endpoint that responds. For `iasZone`, it SHALL read the type’s current zone status. For `windowCovering`, it SHALL read the type’s current covering status. When a registered device advertises Power Configuration (`0x0001`), the slave SHALL also read Battery Percentage Remaining. Sleepy devices that do not answer SHALL leave cached status empty; the coordinator start SHALL still complete. Reads SHALL use the same inbound report path as unsolicited reports. The boot pass SHALL NOT stop after the first registered record if later records for that boot are still being applied.

#### Scenario: Switch after start
- **WHEN** the coordinator starts and a registered `onOff` device with `channels` `1` answers ON
- **THEN** the host receives an `ON` report for that IEEE so the Devices table can show a light circle

#### Scenario: Multi-channel after start
- **WHEN** the coordinator starts and a registered `onOff` device has `channels` `3`
- **THEN** the slave reads On/Off on endpoints 1, 2, and 3

#### Scenario: Four-channel relay channels 1 and 3
- **WHEN** the coordinator starts, a registered `onOff` device has `channels` `4`, endpoints 1 and 3 answer ON, and endpoints 2 and 4 answer OFF
- **THEN** the host receives ON for endpoints 1 and 3 and OFF for endpoints 2 and 4

#### Scenario: Unanswered channel stays unset
- **WHEN** endpoint 1 of a `channels` `4` `onOff` device has answered ON and endpoint 3 has not answered yet
- **THEN** the host has no ON status for endpoint 3

#### Scenario: Battery-capable device
- **WHEN** the coordinator starts and a registered device advertises in-cluster `0x0001` and answers 67 percent
- **THEN** the host receives a battery report that decodes to 67 percent

#### Scenario: Sleepy no answer
- **WHEN** the coordinator starts and a registered device does not answer the read
- **THEN** the coordinator remains running and that device’s status and battery stay unknown until a later report

### Requirement: Radio sends cluster-specific ZCL commands
When the slave executes a host cluster-command request, it SHALL send a cluster-specific ZCL command (not a write-attribute and not On/Off cluster-specific unless that cluster and command were given) to the given IEEE and endpoint. One in-flight device-control command per IEEE and endpoint SHALL include this send the same way as on/off and write-attribute. A missing short address SHALL fail that send without stopping the coordinator.

#### Scenario: Window covering move
- **WHEN** the slave is asked to send cluster `0x0102` command `0x00` to a known short address and endpoint
- **THEN** that device receives the Up/Open covering command on the air

#### Scenario: No short address
- **WHEN** the slave is asked to send a cluster command to a registered IEEE that has no short address
- **THEN** the coordinator stays up and that command is not delivered

### Requirement: ZCL send uses a timed Zigbee lock
When the slave sends a cluster-specific ZCL command for a host device-control request, it MUST acquire the Zigbee stack lock with a finite timeout. If the lock is not obtained in time, that send MUST fail without stopping the coordinator and MUST leave SPI drain and the slave main loop able to continue. The slave MUST NOT wait forever on that lock for this path.

#### Scenario: Lock busy fails one send
- **WHEN** a host cluster-command request runs while the Zigbee lock is held longer than the send timeout
- **THEN** that send fails, the coordinator keeps running, and later SPI or radio work can still proceed

#### Scenario: Lock free sends command
- **WHEN** a host cluster-command request runs and the Zigbee lock is available within the timeout
- **THEN** the slave sends the cluster-specific ZCL command as before
### Requirement: ZigBee settings apply without host restart

When the host persists new Zigbee CHANNEL or PERMIT_JOIN_ON_BOOT_SEC values from the console, the host MUST push those settings to the radio slave and the slave MUST apply them without restarting the host/router. If the Zigbee coordinator is already running on the slave, the slave MUST still apply the new channel (and related settings) rather than ignoring a second settings push.

#### Scenario: Channel change while radio running

- **WHEN** the Zigbee coordinator is already running and the operator saves a new valid CHANNEL
- **THEN** the host does not restart and the slave radio uses the new channel

#### Scenario: Permit-join on boot stored

- **WHEN** the operator saves a new PERMIT_JOIN_ON_BOOT_SEC value
- **THEN** the value is persisted and delivered to the slave without a host restart

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

### Requirement: Deferred outgoing device-command table holds 128 slots
The slave MUST retain a deferred outgoing Zigbee device-control table of at least **128** distinct IEEE and destination-endpoint slots. Newer commands for an existing IEEE+endpoint MUST replace that slot. A new destination MUST NOT be allocated when the table is already full. One-in-flight-per-destination radio rules are unchanged.

#### Scenario: One hundred twenty-eight distinct destinations deferred
- **WHEN** the host delivers deferred device-control commands for 128 different IEEE+endpoint pairs before the radio has drained them
- **THEN** the slave retains all 128 deferred destinations without rejecting the 128th for capacity

#### Scenario: One hundred twenty-ninth distinct destination rejected
- **WHEN** the deferred table already holds 128 distinct IEEE+endpoint destinations and a device-control command arrives for a new IEEE+endpoint
- **THEN** the slave does not allocate a 129th slot and does not put that new destination’s command into the deferred table

#### Scenario: Same destination still replaces
- **WHEN** the deferred table is full at 128 destinations and a newer on/off arrives for an IEEE+endpoint that already has a deferred slot
- **THEN** that slot’s deferred payload is replaced with the newer command

### Requirement: Coordinator TX power is explicitly 20 dBm
After the slave Zigbee coordinator has started successfully for the current boot, the slave MUST set IEEE 802.15.4 transmit power to **+20 dBm** via the Zigbee stack TX-power API (`esp_zb_set_tx_power` or equivalent). The slave MUST NOT leave TX power solely to an implicit stack default. The slave SHOULD log the configured TX power (and read-back when the stack provides it) at info level. This requirement does not require a settings UI or a persisted power preference.

#### Scenario: Power set after successful start
- **WHEN** `Zigbee.begin` for the coordinator succeeds on the slave
- **THEN** the slave sets TX power to +20 dBm before coordinator start is reported as complete to the rest of the firmware

#### Scenario: Start failure leaves radio unused
- **WHEN** `Zigbee.begin` for the coordinator fails
- **THEN** the slave does not claim a successful TX-power configuration for that boot and does not start the coordinator

#### Scenario: Operator can see power in the log
- **WHEN** the coordinator has started and TX power has been set
- **THEN** the slave log includes an info line that states the TX power in dBm (including the value returned by a get API when available)

### Requirement: Seed short↔IEEE from synced last-known short
When the slave applies a registered Zigbee device sync entry that includes a usable last-known NWK short (non-zero and not `0xFFFF`), the slave SHALL associate that short with that IEEE in its runtime short↔IEEE map before or as it queues boot status reads. This SHALL apply to every registered Zigbee device kind using the same registry sync path. The slave SHALL NOT require the Zigbee stack neighbor table to already contain that mapping.

#### Scenario: After reboot with stored short
- **WHEN** the host syncs a registered sensor IEEE with last-known short `0xDBBC` after coordinator start
- **THEN** the slave can resolve short `0xDBBC` to that IEEE without a new join

#### Scenario: Unknown short skipped
- **WHEN** a sync entry has last-known short `0`
- **THEN** the slave does not invent a short mapping for that IEEE from the sync alone

### Requirement: Resolve report IEEE from last-known short map
When a Zigbee attribute or zone report arrives with a short source and the stack has no usable IEEE for that short, the slave SHALL resolve IEEE from its runtime short↔IEEE map (including last-known seeds) before forwarding. On success it SHALL send that IEEE with the short; on failure it MAY send an all-zero IEEE. This applies to every device kind on the shared report path.

#### Scenario: Sleepy report after coordinator reboot
- **WHEN** a registered device reports humidity on short `0xDBBC` after a coordinator reboot and the stack IEEE table misses that short, but the slave has last-known short `0xDBBC` for that IEEE
- **THEN** the host receives the report with that IEEE (not all zeros) and can map it to the registered device

#### Scenario: Truly unknown short stays unmapped
- **WHEN** a report arrives on a short that is not in the slave short↔IEEE map and the stack has no IEEE
- **THEN** the forwarded report has an all-zero IEEE
