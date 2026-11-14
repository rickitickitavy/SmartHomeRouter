# runtime-heap-allocation Specification

## Purpose

Constrains host runtime caches and console binding storage so heap is reserved only for configured channel counts and for optional blocks that are actually present, without changing external JSON or control behavior.

## Requirements

### Requirement: Channel status table matches device channel count

The host MUST size each device’s live channel-status table to that device’s configured channel count (at least 1; at most the existing channel maximum). The host MUST NOT pre-size every device to the global channel maximum when the device declares a smaller count.

#### Scenario: Single-channel device

- **WHEN** a registered device has `channels` equal to 1
- **THEN** the host’s live status table for that device holds capacity for exactly 1 channel

#### Scenario: Multi-channel device

- **WHEN** a registered device has `channels` equal to 4
- **THEN** the host’s live status table for that device holds capacity for exactly 4 channels

### Requirement: Status text is heap-sized to the value

The host MUST NOT embed a fixed userData-max (384-byte) status buffer in every channel-status slot. Status text MUST be stored in heap storage sized to the current value (subject to the existing userData value-length clamp when copying). An empty or unset status MUST NOT keep a large unused buffer reserved for that slot.

#### Scenario: Short on/off status

- **WHEN** a channel reports a short status such as `ON` or `OFF`
- **THEN** the host stores that status without retaining a 384-byte empty reserve for that slot

#### Scenario: Cleared status

- **WHEN** a channel status is cleared or never set
- **THEN** the host does not keep a userData-max buffer allocated for that slot’s status text

### Requirement: Covering extras only when used

The host MUST NOT reserve fixed covering-move, covering-position, and discrete-status buffers on every channel-status slot. Those extras MUST be allocated only when that endpoint actually stores covering or discrete status.

#### Scenario: On/off channel has no covering buffers

- **WHEN** a channel only stores a simple state string and never a covering or discrete value
- **THEN** the host does not keep covering-move, covering-position, or discrete-status buffers for that slot

### Requirement: Manufacturer and model are optional heap strings

The host live-device cache MUST keep manufacturer and model only as optional heap strings. Fixed 32-byte arrays MUST NOT be reserved on every cache slot. Empty values MUST use no heap (or equivalent zero-cost empty). Non-empty values from join/reporting MUST still be available to the bound-device list JSON.

#### Scenario: MQTT device without manufacturer

- **WHEN** a cached device has never received manufacturer or model text
- **THEN** the cache slot does not reserve 64 bytes of empty manufacturer/model storage

#### Scenario: Join still exposes manufacturer and model

- **WHEN** a Zigbee join provides manufacturer and model
- **THEN** the bound-device list JSON still includes those strings for that IEEE

### Requirement: Console binding optional view blocks

A persisted console binding MUST allocate gauge configuration storage only when its view style is `gauge`. Icon-specific configuration MUST be allocated only when view style is `icon`. Logical-threshold storage MUST be allocated only when the binding uses logical conversion. Load and save JSON shapes for those fields MUST remain compatible with existing consoles.

#### Scenario: Default binding omits gauge record

- **WHEN** a binding loads with view style `default` and no gauge object
- **THEN** the host does not allocate a gauge configuration record for that binding

#### Scenario: Gauge binding keeps config

- **WHEN** a binding loads with view style `gauge` and a gauge object
- **THEN** the host stores type, min, max, and ranges, and round-trips them on save

#### Scenario: Icon binding keeps icon config

- **WHEN** a binding loads with view style `icon` and an icon name
- **THEN** the host stores icon-specific fields and round-trips them on save

### Requirement: Camera settings and images stay off internal RAM
Camera registry records and their owned strings MUST be allocated from PSRAM. Any host-captured or host-held camera image (including email snapshot fetch) MUST be allocated only from PSRAM. The host MUST NOT keep those image bytes on internal RAM. If PSRAM cannot satisfy the allocation, the host MUST fail that path without falling back to internal RAM for the image or registry blob.

#### Scenario: Registry in PSRAM
- **WHEN** the camera registry loads on a host with PSRAM
- **THEN** camera records are not kept as an internal-heap-resident table

#### Scenario: Image never on internal RAM
- **WHEN** the host fetches a camera JPEG for email attach
- **THEN** the JPEG buffer is PSRAM-backed or the fetch is abandoned without an internal-RAM image buffer

### Requirement: History slab stays in PSRAM
The host history page slab, series entry metadata used for page directories, and history data pages SHALL be allocated from PSRAM. The host MUST NOT place the history slab on internal RAM. If PSRAM cannot satisfy the configured Maximum history size slab at boot, the host SHALL leave history unavailable (no internal-RAM fallback for the slab) and SHALL still boot other services.

#### Scenario: No internal fallback for slab
- **WHEN** PSRAM cannot allocate the configured history slab at boot
- **THEN** the host does not allocate that slab from internal heap and history storage remains unavailable

### Requirement: Host log ring target is 128 KiB
On non-P4 hosts, the in-memory log ring target capacity in PSRAM SHALL remain **128 KiB** (131072 bytes). On the ESP32-P4 host, the log ring target capacity in PSRAM SHALL be **512 KiB** (524288 bytes). The existing smaller internal-RAM fallback path when PSRAM is unavailable MAY remain. Download-full-log and on-screen log tail behavior SHALL continue to use the actual ring capacity.

#### Scenario: Ring targets 128 KiB with PSRAM on non-P4
- **WHEN** a non-P4 host starts with PSRAM available for the log ring
- **THEN** the log ring capacity is 128 KiB

#### Scenario: Ring targets 512 KiB with PSRAM on P4
- **WHEN** the P4 host starts with PSRAM available for the log ring
- **THEN** the log ring capacity is 512 KiB
