# Spec Delta

## MODIFIED Requirements

### Requirement: Host log ring target is 128 KiB
On non-P4 hosts, the in-memory log ring target capacity in PSRAM SHALL remain **128 KiB** (131072 bytes). On the ESP32-P4 host, the log ring target capacity in PSRAM SHALL be **512 KiB** (524288 bytes). The existing smaller internal-RAM fallback path when PSRAM is unavailable MAY remain. Download-full-log and on-screen log tail behavior SHALL continue to use the actual ring capacity.

#### Scenario: Ring targets 128 KiB with PSRAM on non-P4
- **WHEN** a non-P4 host starts with PSRAM available for the log ring
- **THEN** the log ring capacity is 128 KiB

#### Scenario: Ring targets 512 KiB with PSRAM on P4
- **WHEN** the P4 host starts with PSRAM available for the log ring
- **THEN** the log ring capacity is 512 KiB
