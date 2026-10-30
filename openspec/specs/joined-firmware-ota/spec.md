# joined-firmware-ota Specification

## Purpose

Defines the joined ZIP firmware package (`slave.bin` + `host.bin`) and how join/split tools and the gateway Update path use that single file to program both chips.

## Requirements

### Requirement: Joined firmware package format
The product SHALL define a single **joined firmware package** file that contains both the slave application image and the host application image. The package SHALL be a **ZIP** archive whose members are named exactly `slave.bin` and `host.bin`. Member order in the archive SHALL NOT matter. The package MAY use ZIP DEFLATE or ZIP STORE; tools and the host MUST accept both.

#### Scenario: Valid joined package
- **WHEN** a ZIP contains non-empty `slave.bin` and `host.bin` members
- **THEN** the package is accepted as a joined firmware package for Update

#### Scenario: Missing member rejected
- **WHEN** a ZIP is missing `slave.bin` or `host.bin`
- **THEN** the Update path rejects the file and does not program either chip

### Requirement: Join and split tools
The repository SHALL provide scripts (or documented commands) to **join** a host `.bin` and a slave `.bin` into the joined ZIP and to **split** a joined ZIP back into `host.bin` and `slave.bin`. Those tools SHALL be usable offline without the device.

#### Scenario: Join
- **WHEN** a developer runs the join tool with host and slave application binaries
- **THEN** the tool writes a joined ZIP that the web Update tab can accept

#### Scenario: Split
- **WHEN** a developer runs the split tool on a joined ZIP
- **THEN** the tool writes separate `host.bin` and `slave.bin` files matching the archive members

### Requirement: Web Update uses one joined file
The System → Update tab SHALL continue to accept a **single** file upload. That file SHALL be the joined firmware package. After a valid receive, the host MUST extract `slave.bin`, program the slave over SPI first, and only after slave success extract and program `host.bin`, then restart. Failure before host apply MUST leave the running host application unchanged.

#### Scenario: Successful joined update
- **WHEN** the operator uploads a valid joined ZIP and slave then host commits succeed
- **THEN** both chips run the new images after the host restart completes

#### Scenario: Slave fails inside joined update
- **WHEN** the joined ZIP is accepted but slave programming fails
- **THEN** the host does not apply `host.bin` and reports failure

### Requirement: Slave SPI transfer is exclusive (Zigbee and log quiet)

During each joined-update slave SPI image transfer, the slave SHALL keep SPI exclusive for firmware begin/chunk/end and OTA command results: clear outbound and deferred Zigbee queues on begin, drop log and Zigbee event packets toward the host while applying, and skip Zigbee dispatch/deferred radio apply in the main loop for that window. The begin/chunk/end SPI protocol itself SHALL remain intact.

#### Scenario: Quiet while streaming slave.bin

- **WHEN** the host is streaming `slave.bin` over SPI OTA frames
- **THEN** the slave does not interleave log or Zigbee event packets on SPI and does not apply deferred Zigbee radio work until the apply window ends

### Requirement: Host verifies slave version after SPI transfer before host apply

After the host finishes transferring `slave.bin` over SPI and the slave commits that image, the host SHALL wait until it learns the slave’s running firmware version and that version matches the expected package version when `version.txt` is present (or differs from the pre-transfer version when `version.txt` is absent). Only after that verification succeeds MAY the host apply `host.bin` or report slave-only success. Each wait for a version sample SHALL be at most 20 seconds. The host MAY run at most **3** full SPI transfer + verify cycles for one joined update.

#### Scenario: Expected version observed after first transfer

- **WHEN** slave SPI transfer and commit succeed and the host later reads the expected slave firmware version within the wait window
- **THEN** the host proceeds to host apply (or slave-only success) without sending the slave image again

#### Scenario: Three full cycles exhaust

- **WHEN** after each of 3 SPI transfers the host waits 20 seconds and never reads the expected slave firmware version
- **THEN** the joined update fails and the host does not apply `host.bin`

### Requirement: Failed version verify reboots then full SPI resend

If 20 seconds pass after a SPI transfer without the expected slave firmware version (including no sample, still the pre-transfer version, or any other mismatch), and fewer than 3 SPI cycles have completed, the host SHALL reset the slave, wait until the slave is ready (`Normal`), then start a **full** SPI re-transfer of `slave.bin` from the beginning. The host MUST NOT return to another version-only wait without that SPI re-transfer. The SPI begin/chunk/end protocol itself is unchanged.

#### Scenario: Wrong or stuck version after wait triggers full cycle

- **WHEN** 20 seconds pass after a transfer without the expected version and at least one SPI cycle remains
- **THEN** the host resets the slave, waits until ready, and re-transfers `slave.bin` over SPI from the start

#### Scenario: Match during wait skips resend

- **WHEN** within the 20 second wait the host reads the expected slave firmware version
- **THEN** the host proceeds without reboot or SPI re-upload

#### Scenario: Must not resend on ready alone before verify deadline

- **WHEN** the slave is ready during the 20 second post-transfer wait but the version is not yet the expected value
- **THEN** the host keeps waiting for the version (or the deadline) and does not start SPI re-upload solely because the link is Normal

### Requirement: Successful slave commit MUST NOT trigger redundant re-upload

If the slave already runs the expected firmware version after a completed SPI transfer and commit, the host MUST NOT re-transfer the same `slave.bin` solely because an earlier version read failed or was stale.

#### Scenario: Stale host cache after successful slave commit

- **WHEN** the slave has committed the new image and a later successful version read (including after a verify reset) shows the expected version
- **THEN** the host does not start another full slave SPI upload cycle for that same update

#### Scenario: Router reboot after first transfer

- **WHEN** the operator power-cycles the gateway after the first successful slave transfer and the slave reports the new version on boot
- **THEN** that outcome is treated as evidence the first transfer already succeeded (no requirement that the host have re-uploaded in between)
