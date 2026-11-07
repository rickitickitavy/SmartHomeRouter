## MODIFIED Requirements

### Requirement: Joined firmware package format
The product SHALL define a single **joined firmware package** file that contains both the slave application image and the host application image. The package SHALL be a **ZIP** archive whose members are named exactly `slave.bin` and `host.bin`. Member order in the archive SHALL NOT matter. The package MAY use ZIP DEFLATE or ZIP STORE; tools and the host MUST accept both. The package SHALL also carry **two** version members: `host.version.txt` and `slave.version.txt` (each a single trimmed version line). For backward compatibility, if `slave.version.txt` is absent but `version.txt` is present, the host SHALL treat `version.txt` as the slave package version.

#### Scenario: Valid joined package with two versions
- **WHEN** a ZIP contains non-empty `slave.bin` and `host.bin` plus `host.version.txt` and `slave.version.txt`
- **THEN** the package is accepted as a joined firmware package for Update and both expected versions are known

#### Scenario: Missing member rejected
- **WHEN** a ZIP is missing `slave.bin` or `host.bin`
- **THEN** the Update path rejects the file and does not program either chip

#### Scenario: Legacy version.txt maps to slave
- **WHEN** a ZIP has `version.txt` but no `slave.version.txt`
- **THEN** the host uses `version.txt` as the expected slave package version

### Requirement: Join and split tools
The repository SHALL provide scripts (or documented commands) to **join** a host `.bin` and a slave `.bin` into the joined ZIP and to **split** a joined ZIP back into `host.bin` and `slave.bin`. Those tools SHALL be usable offline without the device. Join SHALL embed `host.version.txt` from `FIRMWARE_VERSION_HOST` and `slave.version.txt` from `FIRMWARE_VERSION_SLAVE` (defaults read from `Defines.h` when not overridden).

#### Scenario: Join
- **WHEN** a developer runs the join tool with host and slave application binaries
- **THEN** the tool writes a joined ZIP that the web Update tab can accept and that contains both version members

#### Scenario: Split
- **WHEN** a developer runs the split tool on a joined ZIP
- **THEN** the tool writes separate `host.bin` and `slave.bin` files matching the archive members

### Requirement: Web Update uses one joined file
The System → Update tab SHALL continue to accept a **single** file upload. That file SHALL be the joined firmware package. After a valid receive, the host MUST decide per chip whether an update is needed (see skip-when-equal). When the slave needs an update, the host MUST extract `slave.bin`, program the slave over SPI first, and only after slave success (or slave skip) extract and program `host.bin` when the host needs an update, then restart if the host image was applied. Failure before host apply MUST leave the running host application unchanged.

#### Scenario: Successful joined update
- **WHEN** the operator uploads a valid joined ZIP and any required slave then host commits succeed
- **THEN** both chips run the package images after any required host restart completes

#### Scenario: Slave fails inside joined update
- **WHEN** the joined ZIP is accepted, slave update is required, but slave programming fails
- **THEN** the host does not apply `host.bin` and reports failure

### Requirement: Host verifies slave version after SPI transfer before host apply

After the host finishes transferring `slave.bin` over SPI and the slave commits that image, the host SHALL wait until it learns the slave’s running firmware version and that version matches the expected **slave** package version when a slave version member is present (or differs from the pre-transfer version when no slave version member is present). Only after that verification succeeds MAY the host apply `host.bin` (when host update is required) or report slave-only success. Each wait for a version sample SHALL be at most 20 seconds. The host MAY run at most **3** full SPI transfer + verify cycles for one joined update. This requirement applies only when a slave transfer was performed (not when slave update was skipped).

#### Scenario: Expected version observed after first transfer

- **WHEN** slave SPI transfer and commit succeed and the host later reads the expected slave firmware version within the wait window
- **THEN** the host proceeds to host apply or completion without sending the slave image again

#### Scenario: Three full cycles exhaust

- **WHEN** after each of 3 SPI transfers the host waits 20 seconds and never reads the expected slave firmware version
- **THEN** the joined update fails and the host does not apply `host.bin`

## ADDED Requirements

### Requirement: Independent host and slave firmware version macros
The firmware SHALL define distinct `FIRMWARE_VERSION_HOST` and `FIRMWARE_VERSION_SLAVE` strings (`MAJOR.MINOR.BUILD`). The host application SHALL identify itself with `FIRMWARE_VERSION_HOST`. The slave application SHALL identify itself with `FIRMWARE_VERSION_SLAVE`. Initially both SHALL equal the previously shared version string. Status and OTA expectations SHALL use the matching chip’s macro/package version, not a single shared string for both chips.

#### Scenario: Host and slave can differ
- **WHEN** `FIRMWARE_VERSION_HOST` is `4.2.03` and `FIRMWARE_VERSION_SLAVE` is `4.2.02`
- **THEN** the host reports `4.2.03` as its version and the slave reports `4.2.02` as its version

### Requirement: Skip chip update when package version already matches
Before starting slave SPI programming, if the package includes a slave image and the slave’s running version equals the package slave version, the host SHALL skip slave transfer and verify for that update. Before applying `host.bin`, if the package includes a host image and the host’s running version equals the package host version, the host SHALL skip host apply (and SHALL NOT force a host restart solely for that skipped apply). Equality is after trim of both strings. When a chip is skipped, the other chip MAY still update.

#### Scenario: Slave already current
- **WHEN** the joined package slave version equals the running slave version and the host package version differs from the running host version
- **THEN** the host skips slave SPI, applies `host.bin`, and restarts the host as required for host apply

#### Scenario: Host already current
- **WHEN** the joined package host version equals the running host version and the slave package version differs from the running slave version
- **THEN** the host updates the slave over SPI (with existing verify rules) and does not apply `host.bin`

#### Scenario: Both already current
- **WHEN** both package versions equal the corresponding running versions
- **THEN** the host programs neither chip and reports a successful no-op update
