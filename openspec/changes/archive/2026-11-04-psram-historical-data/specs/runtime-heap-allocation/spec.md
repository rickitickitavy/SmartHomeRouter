# Spec Delta

## ADDED Requirements

### Requirement: History slab stays in PSRAM
The host history page slab, series entry metadata used for page directories, and history data pages SHALL be allocated from PSRAM. The host MUST NOT place the history slab on internal RAM. If PSRAM cannot satisfy the configured Maximum history size slab at boot, the host SHALL leave history unavailable (no internal-RAM fallback for the slab) and SHALL still boot other services.

#### Scenario: No internal fallback for slab
- **WHEN** PSRAM cannot allocate the configured history slab at boot
- **THEN** the host does not allocate that slab from internal heap and history storage remains unavailable

### Requirement: Host log ring target is 128 KiB
The host in-memory log ring target capacity in PSRAM SHALL be **128 KiB** (131072 bytes). The existing smaller internal-RAM fallback path when PSRAM is unavailable MAY remain. Download-full-log and on-screen log tail behavior SHALL continue to use the actual ring capacity.

#### Scenario: Ring targets 128 KiB with PSRAM
- **WHEN** the host starts with PSRAM available for the log ring
- **THEN** the log ring capacity is 128 KiB rather than 512 KiB
