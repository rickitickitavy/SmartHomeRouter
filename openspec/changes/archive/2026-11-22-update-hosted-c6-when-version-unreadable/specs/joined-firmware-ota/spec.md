## MODIFIED Requirements

### Requirement: Skip chip update when package version already matches
Before starting slave SPI programming, if the package includes a slave image and the slave’s running version equals the package slave version, the host SHALL skip slave transfer and verify for that update. Before applying `host.bin`, if the package includes a host image and the host’s running version equals the package host version, the host SHALL skip host apply (and SHALL NOT force a host restart solely for that skipped apply). Equality is after trim of both strings. When a chip is skipped, the other chip MAY still update. On P4, before Hosted C6 SDIO programming: absent `hosted-c6.bin` → skip; present with matching running version → skip; present with different version → apply; present with valid `hosted-c6.version.txt` but **running Hosted C6 version unreadable** → **apply** (do not fail the Hosted step for that reason alone); present but package version text missing/empty → fail Hosted step (do not program).

#### Scenario: Slave already current
- **WHEN** the joined package slave version equals the running slave version and the host package version differs from the running host version
- **THEN** the host skips slave SPI, applies `host.bin`, and restarts the host as required for host apply

#### Scenario: Host already current
- **WHEN** the joined package host version equals the running host version and the slave package version differs from the running slave version
- **THEN** the host updates the slave over SPI (with existing verify rules) and does not apply `host.bin`

#### Scenario: Both already current
- **WHEN** both package versions equal the corresponding running versions
- **THEN** the host programs neither chip and reports a successful no-op update

#### Scenario: Hosted absent → skip
- **WHEN** the staged ZIP has no `hosted-c6.bin`
- **THEN** the P4 host does not run Hosted C6 SDIO update

#### Scenario: Hosted match → skip
- **WHEN** `hosted-c6.bin` is present and running version equals `hosted-c6.version.txt`
- **THEN** the P4 host skips Hosted C6 programming

#### Scenario: Hosted mismatch → apply
- **WHEN** `hosted-c6.bin` is present and running version differs from `hosted-c6.version.txt`
- **THEN** the P4 host programs Hosted C6 from that member over SDIO after the ZIP is fully staged

#### Scenario: Hosted running version unreadable → apply
- **WHEN** `hosted-c6.bin` and a non-empty `hosted-c6.version.txt` are present and the host cannot read the running Hosted C6 version
- **THEN** the P4 host still marks Hosted C6 update required and programs that member over SDIO (does not fail prepare solely because the running version was unreadable)
