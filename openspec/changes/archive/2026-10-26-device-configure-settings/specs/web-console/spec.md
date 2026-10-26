# Spec Delta

## ADDED Requirements

### Requirement: Add-device found table shows join manufacturer
The Add-device search dialog found table MUST include a **Manufacturer** column. The cell MUST show the manufacturer string received from the Zigbee device join (the same value returned by the found-devices API), not a mapped enum label. The **Model** column MUST show only the model string from that join.

#### Scenario: Found row shows join manufacturer
- **WHEN** Search finds a device whose join manufacturer string is `_TZ3000_wn65ixz9` and model is `TS0001`
- **THEN** the found table Manufacturer cell shows `_TZ3000_wn65ixz9` and the Model cell shows `TS0001`

### Requirement: Join identity comes from Basic cluster reads
After a Zigbee device joins, the coordinator MUST read Basic cluster attributes ManufacturerName (`0x0004`) and ModelIdentifier (`0x0005`) from a usable endpoint, store the char-string values on the bound device, and emit an updated join event so the found-devices API and manufacturer enum mapping receive those strings. Empty strings from an earlier join MUST NOT overwrite a later non-empty manufacturer or model on the found list.

#### Scenario: Basic identity fills found manufacturer
- **WHEN** a device joins and replies to Basic ManufacturerName with `_TZ3000_wn65ixz9` and ModelIdentifier with `TS0001`
- **THEN** a subsequent found-devices list entry for that IEEE shows manufacturer `_TZ3000_wn65ixz9` and model `TS0001`

### Requirement: Device edit shows readonly manufacturer for Zigbee
The Zigbee device parameters/edit dialog MUST show a readonly **Manufacturer** field immediately after **Device class**. The field MUST display the human manufacturer label from the manufacturer enum (for example `Tuya`), including when Add opens from a found join string such as `_TZ3000_…` (mapped via the same join-string rules as registration). The field MUST be hidden for MQTT devices.

#### Scenario: Edit Zigbee device shows manufacturer label
- **WHEN** the operator opens Edit on a Zigbee device whose stored manufacturer is `tuya`
- **THEN** the Manufacturer field is visible after Device class, readonly, and shows `Tuya`

#### Scenario: Add from found maps join string to label
- **WHEN** the operator presses Add on a found device whose join manufacturer is `_TZ3000_bs93npae`
- **THEN** the Manufacturer field shows `Tuya`, not the raw join string

#### Scenario: MQTT device omits manufacturer field
- **WHEN** the operator opens the MQTT device parameters dialog
- **THEN** the Manufacturer field is hidden

### Requirement: Devices row menu offers Configure when applicable
When the Devices row **…** menu is shown for a registered Zigbee device that has at least one configurable attribute (per `device-configure`), the menu MUST include a **Configure** item in addition to any type actions. When the device has no configurable attributes, **Configure** MUST NOT appear. Choosing **Configure** MUST open the Configure dialog for that IEEE and MUST NOT send an On/Off or covering command.

#### Scenario: Tuya onOff shows Configure
- **WHEN** the operator opens **…** on a Zigbee `onOff` device with manufacturer `tuya` that has catalog settings
- **THEN** the menu includes Configure and type actions such as ON/OFF/TOGGLE

#### Scenario: No catalog means no Configure
- **WHEN** the operator opens **…** on a Zigbee device whose manufacturer and type have an empty config catalog
- **THEN** Configure is absent from the menu

#### Scenario: IAS zone without config
- **WHEN** a Zigbee `iasZone` device has no configurable catalog entries
- **THEN** **…** is still absent when there are no type actions and no Configure (same as today for type actions alone)

### Requirement: Configure dialog loads a typed attribute grid
The Configure dialog MUST show the device name, load configurable attributes into a grid of label + editor controls, and use control types matching each attribute (`bool` checkbox or equivalent, `enum` select, integer number input). The dialog MUST show a busy/loading state while live reads complete, and MUST show an error when the device cannot be read. Cancel/close MUST discard unsaved edits without writing attributes.

#### Scenario: Enum rendered as select
- **WHEN** Configure opens and a catalog entry is type `enum` with choices
- **THEN** that row uses a select listing those choice labels

#### Scenario: Cancel discards edits
- **WHEN** the operator changes a value then cancels the dialog
- **THEN** no Zigbee attribute write is performed

### Requirement: Configure Save writes only changed attributes
Configure Save MUST send only attributes the operator changed relative to the values loaded into the dialog. On success the dialog MUST close or show success and MUST NOT persist those values into the devices table store. On failure the dialog MUST remain open with an error and MUST NOT claim success.

#### Scenario: Partial edit save
- **WHEN** two attributes loaded and the operator changes one then saves
- **THEN** only the changed attribute is written to the device

### Requirement: Configure requires controlDevices
A user without `controlDevices` and without `isAdmin` MUST NOT see Configure on the **…** menu and MUST NOT call Configure load/save APIs successfully. `isAdmin` and users with `controlDevices` MUST be allowed when the device qualifies.

#### Scenario: Simple user blocked
- **WHEN** a simple user without `controlDevices` views Devices
- **THEN** Configure is not available and Configure API calls return 403
