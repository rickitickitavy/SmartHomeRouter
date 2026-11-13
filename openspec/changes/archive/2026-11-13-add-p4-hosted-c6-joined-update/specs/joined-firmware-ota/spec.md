## MODIFIED Requirements

### Requirement: Joined firmware package format
The joined ZIP MAY include optional `hosted-c6.bin` and `hosted-c6.version.txt`. Absence of those members SHALL NOT invalidate an otherwise valid package. Host and/or slave image rules remain as today.

#### Scenario: Optional Hosted members
- **WHEN** a ZIP includes `hosted-c6.bin` and `hosted-c6.version.txt` plus host and/or slave images
- **THEN** the package is accepted

#### Scenario: Missing Hosted members still valid
- **WHEN** a ZIP has no `hosted-c6.bin`
- **THEN** Update does not require a Hosted C6 step

### Requirement: Skip chip update when package version already matches
On P4, before Hosted C6 SDIO programming: absent `hosted-c6.bin` → skip; present with matching running version → skip; present with different version → apply; present but version text missing/empty or running version unreadable → fail Hosted step (do not program).

#### Scenario: Hosted absent → skip
- **WHEN** the staged ZIP has no `hosted-c6.bin`
- **THEN** the P4 host does not run Hosted C6 SDIO update

#### Scenario: Hosted match → skip
- **WHEN** `hosted-c6.bin` is present and running version equals `hosted-c6.version.txt`
- **THEN** the P4 host skips Hosted C6 programming

#### Scenario: Hosted mismatch → apply
- **WHEN** `hosted-c6.bin` is present and running version differs from `hosted-c6.version.txt`
- **THEN** the P4 host programs Hosted C6 from that member over SDIO after the ZIP is fully staged
