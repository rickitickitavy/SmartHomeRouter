# p4-hosted-c6-ota Specification

## Purpose

P4 host applies optional joined-package Hosted C6 image only when present and needed.

## Requirements

### Requirement: Present and needed only
The P4 host SHALL update the ESP-Hosted Wi‑Fi C6 coprocessor from `hosted-c6.bin` only when that member is in the staged joined package and the running coprocessor version differs from `hosted-c6.version.txt`. It SHALL NOT poll Hosted version on ordinary Status refreshes for this feature.

#### Scenario: Skip when not present
- **WHEN** `hosted-c6.bin` is absent
- **THEN** no Hosted C6 update runs

#### Scenario: Skip when not needed
- **WHEN** the file is present and versions match
- **THEN** no Hosted C6 SDIO transfer runs

#### Scenario: Apply when needed
- **WHEN** the file is present and versions differ
- **THEN** the host applies that image over SDIO in the joined update session
