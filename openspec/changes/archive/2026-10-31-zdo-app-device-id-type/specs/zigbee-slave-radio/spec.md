# Spec Delta

## MODIFIED Requirements

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

## ADDED Requirements

### Requirement: Application Device ID map covers HA and ZLO ids
The slave SHALL map at least these Application Device IDs to gateway types for new devices: On/Off family (`0x0000`, `0x0002`, `0x0009`, `0x0051`, `0x0100`, `0x0103`, `0x010A`, and other On/Off-equivalent HA/ZLO ids used by the product) → `onOff`; dimmable / level / color light and switch ids that the product treats as level or color classes → the corresponding catalog types; shade / window covering ids (`0x0200`–`0x0203`) → `windowCovering`; thermostat `0x0301` → thermostat catalog type; temperature sensor `0x0302` → temperature-measurement catalog type; IAS Zone `0x0402` → `iasZone`. Unlisted Device IDs SHALL map to `unknown` until explicitly added.

#### Scenario: Unlisted Device ID
- **WHEN** Application Device ID is `0x0008` (Range Extender) and no product mapping exists
- **THEN** type is `unknown`
