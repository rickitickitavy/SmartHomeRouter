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

### Requirement: Host verifies slave version after SPI transfer before host apply

After the host finishes transferring `slave.bin` over SPI and the slave commits that image, the host SHALL wait until it learns the slave’s running firmware version and that version matches the expected package version when `version.txt` is present (or differs from the pre-transfer version when `version.txt` is absent). Only after that verification succeeds MAY the host apply `host.bin` or report slave-only success. Each wait for a version sample SHALL be at most 20 seconds.

#### Scenario: Expected version observed after first transfer

- **WHEN** slave SPI transfer and commit succeed and the host later reads the expected slave firmware version within the wait window
- **THEN** the host proceeds to host apply (or slave-only success) without sending the slave image again

#### Scenario: No version response after allowed resets

- **WHEN** the host cannot obtain a slave firmware version sample within 20 seconds after each of 5 no-response resets
- **THEN** the joined update fails and the host does not apply `host.bin`

### Requirement: Version sample required before SPI resend; silence triggers reset

The host MUST NOT start another SPI transfer of `slave.bin` until it has obtained a non-empty firmware version sample from the slave after the previous transfer. If the slave does not respond with a version within 20 seconds, the host SHALL reset the slave and wait again (up to 5 such resets). If a version sample is present and does not match, the host MAY re-transfer `slave.bin` over SPI. If all 5 no-response resets fail to produce a version sample, the update SHALL fail.

#### Scenario: Pre-OTA version during wait is not a resend

- **WHEN** after SPI transfer the host still reads the same firmware version the slave had before the transfer
- **THEN** the host keeps waiting for the expected post-reboot version and does not start SPI re-upload for that sample

#### Scenario: Wrong post-reboot version after wait

- **WHEN** 20 seconds pass and the host has a non-empty version that is neither the expected version nor the pre-transfer version
- **THEN** the host re-transfers `slave.bin` over SPI

#### Scenario: No response or stuck pre-OTA leads to reset not resend

- **WHEN** 20 seconds pass after a transfer (or after a verify reset) without a non-empty version sample, or only with the pre-transfer version
- **THEN** the host resets the slave and does not start SPI re-upload for that timeout

#### Scenario: Five no-response resets exhaust

- **WHEN** the host has already performed 5 no-response resets in the current post-transfer verify cycle and another 20 second wait yields no version sample
- **THEN** the update fails without another SPI upload

#### Scenario: Must not resend on ready alone

- **WHEN** the slave becomes ready but the host has not yet obtained a non-empty version sample
- **THEN** the host does not start SPI re-upload solely because the link is Normal

### Requirement: Successful slave commit MUST NOT trigger redundant re-upload

If the slave already runs the expected firmware version after a completed SPI transfer and commit, the host MUST NOT re-transfer the same `slave.bin` solely because an earlier version read failed or was stale.

#### Scenario: Stale host cache after successful slave commit

- **WHEN** the slave has committed the new image and a later successful version read (including after a verify reset) shows the expected version
- **THEN** the host does not start another full slave SPI upload cycle for that same update

#### Scenario: Router reboot after first transfer

- **WHEN** the operator power-cycles the gateway after the first successful slave transfer and the slave reports the new version on boot
- **THEN** that outcome is treated as evidence the first transfer already succeeded (no requirement that the host have re-uploaded in between)
