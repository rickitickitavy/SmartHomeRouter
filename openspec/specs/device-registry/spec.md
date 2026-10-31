# device-registry Specification

## Purpose

Keeps the operator’s Zigbee device map on the Wi-Fi host, with a reserved main-settings tail, a device list after that reserve, pairing search results in RAM, and EEPROM writes limited to the bytes that changed.

## Requirements

### Requirement: Registered devices live only on the host
The host SHALL persist the registered-device list in its settings store. The Zigbee slave SHALL NOT persist that list. A join report from the slave SHALL NOT by itself create a registered record.

#### Scenario: Join without Save
- **WHEN** the slave reports a joining device and the operator has not saved that IEEE on the host
- **THEN** the device is eligible for the found list and MUST NOT appear as a registered device

#### Scenario: Slave reboot
- **WHEN** the slave reboots after the host has registered a device
- **THEN** the host still has that registered record after its own settings load

### Requirement: Main settings end with a reserved aligned tail
The host settings image SHALL place wifi, mqtt, zigbee, and other current main fields in a main block. After those fields the main block SHALL include a reserved region of 256 bytes. The start of that reserved region SHALL be aligned to 128 bytes from the start of the settings image. The registered device list SHALL begin immediately after the reserved region and SHALL provide 128 slots. Uninitialized reserved bytes SHALL be zero. The 128-slot list SHALL NOT be stored inside the 4 KiB EEPROM main image (it does not fit); it SHALL live on the host immediately after that image in persistent storage.

#### Scenario: Layout after migrate
- **WHEN** settings are saved at the new version
- **THEN** the device list does not overlap the reserved 256-byte tail and the reserved tail starts on a 128-byte boundary

#### Scenario: Reserved bytes unused
- **WHEN** the operator saves only a device record
- **THEN** the reserved tail is left as stored zeros or previous reserved data and is not used as device fields

#### Scenario: One hundred twenty-eight slots
- **WHEN** the host settings are at the new version
- **THEN** the operator can register up to 128 devices and a 129th save is rejected

### Requirement: Settings writes touch only dirty EEPROM bytes
A settings save SHALL program only the EEPROM bytes that differ from the last committed image (or a contiguous dirty span that contains those bytes). A save that changes one registered device SHALL NOT rewrite unchanged main-block bytes. A save that changes only wifi or mqtt SHALL NOT rewrite unchanged device-list bytes.

#### Scenario: Save one new device
- **WHEN** the operator saves a newly registered device and main settings are unchanged
- **THEN** EEPROM writes are limited to the device-list bytes for that save

#### Scenario: Save Wi-Fi only
- **WHEN** the operator saves Wi-Fi settings and the device list is unchanged
- **THEN** EEPROM writes do not include unchanged device-list bytes

### Requirement: Search opens pairing and lists found unregistered devices
Starting a device search from the host SHALL open the slave join window. While search is active, each slave join report for an IEEE that is not already registered SHALL appear in the host found-device list without waiting for a page reload. A found entry SHALL include at least IEEE and a device type (`unknown` if the slave did not send one), and, when the slave sent them, manufacturer and model.

#### Scenario: Search then join
- **WHEN** the operator starts Search and a new unregistered device joins
- **THEN** that device appears in the found list while Search is still open

#### Scenario: Already registered ignored
- **WHEN** a join report arrives for an IEEE that is already registered
- **THEN** that IEEE is not added to the found list

#### Scenario: Found type from join
- **WHEN** an unregistered device joins with classified type `onOff`
- **THEN** the found list entry for that IEEE includes type `onOff`

### Requirement: Registering a found device requires operator Save
Selecting a found device and confirming Add SHALL identify that IEEE for the parameter form. The host SHALL create or update a registered record only when the operator saves the parameter form with a friendly name. Empty MQTT topics MAY be stored empty. Save SHALL fail when the device list has no free slot.

#### Scenario: Save after Add
- **WHEN** the operator Adds a found device, fills a friendly name, and saves
- **THEN** that IEEE is a registered device on the host

#### Scenario: Map full
- **WHEN** every device slot is used and the operator tries to save a new IEEE
- **THEN** the host rejects the save and the list is unchanged

### Requirement: Registered devices store FULL CONTROL
Each registered device record SHALL store a FULL CONTROL flag. The default SHALL be off, which SHALL keep existing command behavior (channels mapping, then on/off/toggle). A save that omits the flag SHALL leave a new record off and SHALL NOT turn an existing record on. The host SHALL persist the flag with the device list. The slave SHALL NOT be required to persist or interpret this flag.

#### Scenario: New device default
- **WHEN** the operator saves a device without a FULL CONTROL value
- **THEN** that record has FULL CONTROL off and MQTT commands still use on/off/toggle

#### Scenario: Flag survives host reload
- **WHEN** a device is saved with FULL CONTROL on and the host reloads settings
- **THEN** that device still has FULL CONTROL on

### Requirement: Registered devices store transport

Each registered device record SHALL store a transport of `zigbee` or `mqtt`. Transport is a persisted device setting: it SHALL appear in `devices.json`, list JSON after reboot, and device APIs. Transport SHALL be set when the record is created and SHALL NOT change on later saves. A save that omits transport on a new Zigbee (found-list) record SHALL store `zigbee`. A save that creates an MQTT device SHALL store `mqtt`. Records loaded without a transport field SHALL be treated as `zigbee`. Records loaded with the legacy id `wifi` SHALL NOT be remapped to `mqtt` (they SHALL fall back like an unrecognized transport).

#### Scenario: Found Zigbee save

- **WHEN** the operator saves a device from the pairing found list
- **THEN** that record has transport `zigbee`

#### Scenario: WiFi create save

- **WHEN** the operator saves a new device from the MQTT DEVICE path
- **THEN** that record has transport `mqtt`

#### Scenario: Legacy record

- **WHEN** the host loads a stored device that has no transport field
- **THEN** list JSON reports transport `zigbee`

#### Scenario: Transport cannot change

- **WHEN** a registered device already has transport `zigbee` and a save request includes transport `mqtt`
- **THEN** the record still has transport `zigbee`

#### Scenario: Legacy wifi id not remapped

- **WHEN** the host loads a stored device with transport string `wifi`
- **THEN** list JSON does not report transport `mqtt` for that record

### Requirement: Registered devices store cluster device type

Each registered device record SHALL store a device type of `unknown`, `onOff`, `iasZone`, `windowCovering`, `userData`, or another catalog type id the gateway persists for Zigbee Application Device ID classification. Type is a persisted device setting: it SHALL appear in `devices.json`, in `SpiCmdSetDevice` / dump payloads for `zigbee` devices, and in list JSON after reboot. For `zigbee` devices, readonly UI SHALL prevent the operator from editing type; it SHALL NOT skip persistence. A new save from a found device SHALL copy the type from that found record. A save that omits type on a new record SHALL store `unknown`. For `zigbee` devices, after the record exists in the registry (any stored type including `unknown`), later joins, Simple Descriptor results, Basic identity reads, and operator saves SHALL leave that type unchanged—even if a later classification differs or the request body includes a different type. The host SHALL NOT update a registered Zigbee type from a later join classification. The slave SHALL persist the same type on its device dump so a host pull cannot replace a known type with `unknown`. For `mqtt` devices, the operator SHALL be allowed to set and later change type among `unknown`, `onOff`, `iasZone`, `windowCovering`, and `userData`; join classification SHALL NOT overwrite an `mqtt` type; the host SHALL NOT require the slave to store `mqtt` types. Type `userData` SHALL map to cluster id `0xFC00`.

#### Scenario: Save from found list

- **WHEN** the operator saves a found device whose join type is `iasZone`
- **THEN** the registered record has type `iasZone`

#### Scenario: Operator cannot overwrite a known type

- **WHEN** a registered `zigbee` device already has type `onOff` and the operator saves parameters with a different type in the request
- **THEN** the record still has type `onOff`

#### Scenario: Registered type stays after later join

- **WHEN** a registered `zigbee` IEEE has type `onOff` and a later join would classify Application Device ID as temperature sensor
- **THEN** the registered record remains `onOff`

#### Scenario: Registered unknown is not filled later

- **WHEN** a registered `zigbee` IEEE is `unknown` and a later join classifies it as `windowCovering`
- **THEN** the registered record remains `unknown`

#### Scenario: Operator sets WiFi type

- **WHEN** a registered `mqtt` device has type `unknown` and the operator saves type `onOff`
- **THEN** the record has type `onOff`

#### Scenario: Operator sets MQTT userData type

- **WHEN** a registered `mqtt` device has type `unknown` and the operator saves type `userData`
- **THEN** the record has type `userData`

### Requirement: Found devices take type from Application Device ID join
While search is active, a found (not yet registered) Zigbee entry’s type SHALL come from the slave join classification based on Application Device ID. Manufacturer and model on the found row SHALL NOT change that type.

#### Scenario: Found row matches Device ID class
- **WHEN** the slave reports a found join with Application Device ID mapped to `onOff`
- **THEN** the found list shows type `onOff` for that IEEE

### Requirement: Device list JSON includes type
Host device list APIs and the persisted device store SHALL include the stored type for each registered device so the console and future actions can read it.

#### Scenario: List after save
- **WHEN** the operator requests the registered device list and a device was saved as `onOff`
- **THEN** that device’s JSON includes type `onOff`

### Requirement: Device list JSON includes runtime status and battery
Host registered-device list APIs SHALL include last known per-endpoint status and battery percent when the host has received those values. Status entries SHALL include the source endpoint (`ep`) and the last type-specific payload for that endpoint. Battery SHALL be an integer percent derived from Power Configuration Battery Percentage Remaining when a valid report exists. These fields are runtime telemetry: they SHALL appear after reboot only if a new report or a start-time read has arrived; they SHALL NOT be written into the persisted device list, `devices.json`, or EEPROM device slots. A list replace, export, or restore SHALL NOT require or store these fields.

#### Scenario: List after on/off report
- **WHEN** the operator requests the registered device list and endpoint 2 of that IEEE last reported `ON`
- **THEN** that device’s JSON includes a status entry for `ep` `2` with payload `ON`

#### Scenario: List after battery report
- **WHEN** the host has a valid battery percentage remaining that decodes to 67 percent
- **THEN** that device’s list JSON includes battery `67`

#### Scenario: Persist does not keep telemetry
- **WHEN** the host saves or exports the device list after it has cached battery and status
- **THEN** the persisted document has no battery or live status fields, and those values are absent after a reboot until a new report or start-time read arrives

### Requirement: Non-MQTT devices store publishToMqtt
Each registered device whose transport is not `mqtt` SHALL store a boolean `publishToMqtt`. The field SHALL appear in `devices.json`, device list JSON after reboot, and device create/update APIs. MQTT-transport devices SHALL NOT store or require this field. When a stored non-MQTT record omits `publishToMqtt`, the host SHALL treat it as `true`. When the host creates a new non-MQTT device record, `publishToMqtt` SHALL default to `false` unless the save body explicitly sets it.

#### Scenario: Legacy zigbee without field
- **WHEN** the host loads a `zigbee` device from `devices.json` with no `publishToMqtt` key
- **THEN** list JSON reports `publishToMqtt` as true

#### Scenario: New zigbee defaults off
- **WHEN** the operator saves a new `zigbee` device and the body omits `publishToMqtt`
- **THEN** the stored record has `publishToMqtt` false

#### Scenario: Explicit save
- **WHEN** the operator saves a non-MQTT device with `publishToMqtt` true
- **THEN** after reload the record still has `publishToMqtt` true

#### Scenario: MQTT transport excluded
- **WHEN** the operator saves or loads an `mqtt` transport device
- **THEN** the record does not require `publishToMqtt` and MQTT-native behavior is unchanged

### Requirement: User-data devices store ordered field schema

Each `userData` device SHALL persist ordered `userDataFields` with `nullable`, unique non-empty `name`, and `type` (`int`/`float`/`bool`/`string`). List order is attr `0x0001` value-slot order. Omitted fields on create store `[]`. Schema MUST round-trip in `devices.json`, list JSON, and device export/import. A device MUST accept at most **24** fields; attempts to store more MUST be rejected or truncated consistently so the persisted count never exceeds 24.

#### Scenario: Save schema then reload

- **WHEN** the operator saves a `userData` device with two fields (nullable off, name `temp`, type `float`) then (nullable on, name `label`, type `string`)
- **THEN** after reboot list JSON for that IEEE includes those two fields in that order with the same nullable, name, and type

#### Scenario: Empty schema allowed

- **WHEN** the operator saves a new `userData` device with no fields
- **THEN** the record has type `userData` and an empty `userDataFields` list

#### Scenario: Twenty-four fields persist

- **WHEN** the operator saves a `userData` device with 24 uniquely named fields
- **THEN** after reload list JSON includes all 24 fields in the saved order

### Requirement: Registered devices store custom commands
Each registered device record MUST persist an ordered list of custom commands (empty by default). Custom commands are settings: they MUST appear in device list JSON, single-device save/read, devices export/import, and after host reboot. Omitted `customCommands` on load MUST become an empty list. The slave MUST NOT be required to store or interpret custom commands.

#### Scenario: Survives reboot
- **WHEN** a device is saved with two custom commands and the host reboots
- **THEN** list JSON still includes those two commands with the same names, parameter flags, and types

#### Scenario: Legacy device without field
- **WHEN** the host loads a stored device that has no `customCommands` field
- **THEN** that record has an empty custom command list

#### Scenario: Export round-trip
- **WHEN** the operator exports devices and imports the same JSON
- **THEN** each device’s custom commands match what was exported

### Requirement: Registered devices store manufacturer enum
Each registered device record MUST store a manufacturer enum (`unknown`, `tuya`, `aqara`, `philipsHue`, `ikea`, `sonoff`, `ledvance`, `innr`, `gledopto`, `sengled`, `thirdReality`, `heiman`, `legrand`, `schneider`, `namron`, `develco`, `zbeacon`) as a persisted settings field through `devices.json`, list JSON after reboot, single-device save/read, and export/import. The slave MUST NOT be required to store the enum.

#### Scenario: Tuya join maps and persists
- **WHEN** the operator saves a found Zigbee device whose join manufacturer is a Tuya string
- **THEN** the registered record stores manufacturer `tuya` and that value is present after reboot and in export JSON

#### Scenario: New save omits manufacturer
- **WHEN** a new record is saved without a manufacturer field
- **THEN** the stored manufacturer is `unknown`

#### Scenario: Legacy record without field
- **WHEN** the host loads a device record with no manufacturer key
- **THEN** the in-memory manufacturer is `unknown`

#### Scenario: Parameter save does not clobber known manufacturer
- **WHEN** a Zigbee device already stores manufacturer `aqara` and the operator saves parameters with a different manufacturer in the body
- **THEN** the stored manufacturer remains `aqara`

#### Scenario: MQTT default
- **WHEN** an MQTT device is created without a manufacturer
- **THEN** the stored manufacturer is `unknown`

### Requirement: Device list JSON includes manufacturer
Host registered-device list APIs and the persisted device store MUST include the stored manufacturer enum JSON id for each registered device so the console can decide Configure eligibility and display the brand.

#### Scenario: List shows manufacturer
- **WHEN** the console loads the devices list for a registered Tuya device
- **THEN** that device’s JSON includes `"manufacturer":"tuya"` (or the project’s chosen JSON id for that enum)

### Requirement: MQTT devices store zigbeeProtocol
Each registered `mqtt` device record SHALL store a boolean `zigbeeProtocol`. The field SHALL appear in `devices.json`, device list JSON after reboot, single-device save/read, and export/import. Default SHALL be false when omitted on create or load. Later saves of an existing `mqtt` record SHALL accept changes to `zigbeeProtocol` (it is not create-only). Non-`mqtt` records SHALL NOT store or require this field. The slave SHALL NOT be required to persist or interpret this flag.

#### Scenario: New MQTT omits flag
- **WHEN** the operator creates an `mqtt` device without `zigbeeProtocol`
- **THEN** the stored record has `zigbeeProtocol` false

#### Scenario: Persist on
- **WHEN** the operator saves an `mqtt` device with `zigbeeProtocol` true
- **THEN** after reload and export the record still has `zigbeeProtocol` true

#### Scenario: Later edit changes flag
- **WHEN** a stored `mqtt` device has `zigbeeProtocol` false and a later save sets `zigbeeProtocol` true
- **THEN** the stored record has `zigbeeProtocol` true after reload

#### Scenario: Zigbee transport excluded
- **WHEN** the operator saves or loads a `zigbee` transport device
- **THEN** the record does not require `zigbeeProtocol`

### Requirement: Devices store reportableData
Each registered device record (Zigbee or MQTT) SHALL store an optional `reportableData` array of at most 16 rows. Each row SHALL have `cluster` (uint16), `attribute` (uint16), and `type` (`int` | `float` | `bool` | `string`). Storage SHALL allocate memory only for the used row count. The field SHALL round-trip through `devices.json`, list JSON, single-device save/read, and export/import. Omitted on load SHALL mean an empty list. The slave SHALL NOT be required to persist this array (host-only, like `userDataFields`).

#### Scenario: Empty by default
- **WHEN** the operator creates a device without `reportableData`
- **THEN** the stored record has an empty reportable list unless create auto-fill seeds rows

#### Scenario: Persist two rows
- **WHEN** the operator saves a device with two reportable rows
- **THEN** after reload the record still has those two rows and host memory holds only two row slots

#### Scenario: Create auto-fill
- **WHEN** a new device is created with manufacturer Zbeacon (or a discovered measurement class) and the body omits `reportableData`
- **THEN** the host seeds temperature, humidity, and/or battery rows from manufacturer catalog merged with discovered class/cluster hints

### Requirement: Manufacturer includes Zbeacon
The device manufacturer catalog SHALL include `zbeacon` (label Zbeacon). Join identity strings containing `zbeacon` (case-insensitive) SHALL map to that manufacturer.

#### Scenario: Join maps Zbeacon
- **WHEN** a found device reports manufacturer name `Zbeacon`
- **THEN** the stored manufacturer id is `zbeacon`

### Requirement: Registered Zigbee devices store last-known NWK short
Each registered Zigbee device record SHALL store a last-known NWK short address (16-bit). Missing or unknown SHALL be stored as `0`. The field SHALL appear in `devices.json` and list JSON after reboot (for example as `nwk`). MQTT-transport records MAY omit a usable short. The slave SHALL NOT be the durable store for this field.

#### Scenario: Short survives host reload
- **WHEN** a Zigbee device was last seen on short `0xA369` and the host reloads the device store
- **THEN** that IEEE’s registered record still has last-known short `0xA369`

#### Scenario: Unknown default
- **WHEN** the operator saves a new Zigbee device that has never reported a short
- **THEN** that record’s last-known short is `0`

### Requirement: Host updates last-known short from radio identity
When the host receives a slave join, identity, or attribute/zone report for a registered Zigbee IEEE with a usable short (non-zero and not `0xFFFF`), the host SHALL set that IEEE’s stored last-known short to that value. If another registered Zigbee IEEE still stores the same short, the host SHALL clear that other record’s last-known short to `0`. The host SHALL persist the change with the device store.

#### Scenario: Report refreshes short
- **WHEN** a registered IEEE reports with short `0xDBBC`
- **THEN** that IEEE’s stored last-known short becomes `0xDBBC`

#### Scenario: Short moves to another IEEE
- **WHEN** IEEE A stored short `0xDBBC` and IEEE B later reports with short `0xDBBC`
- **THEN** IEEE B stores `0xDBBC` and IEEE A’s last-known short becomes `0`
