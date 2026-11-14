# sd-card-storage Specification

## Purpose

Brings up the ESP32-P4 host onboard MicroSD for durable storage when a card is present, including power, mount, format-if-needed, capacity reporting, and operator-visible lifecycle logging.

## Requirements

### Requirement: P4 host probes MicroSD at boot
On the ESP32-P4 host only, during boot after basic logging is available, the host SHALL attempt to power and initialize the onboard MicroSD slot. The host SHALL log whether a card was detected, mount succeeded or failed, and whether a format was required. Non-P4 hosts SHALL NOT attempt this bring-up.

#### Scenario: Card present and already formatted
- **WHEN** a formatted card is in the P4 MicroSD slot at boot
- **THEN** the host mounts the filesystem and logs successful SD bring-up

#### Scenario: No card
- **WHEN** the P4 boot SD probe finds no card
- **THEN** the host logs that SD is absent and continues without an SD mount

### Requirement: Unformatted card is formatted then mounted
When the P4 host detects a card that is not a usable filesystem for this product, it SHALL format the card with a filesystem suitable for that card’s capacity (including support for cards up to 64 GB), log the format action and result, then mount. Format MUST run only when the card is present and unusable as-is.

#### Scenario: Blank card formatted
- **WHEN** a present card fails mount because it is unformatted
- **THEN** the host formats it, logs the format, mounts it, and logs mount success or failure

### Requirement: Filesystem supports cards up to 64 GB
The P4 host SD filesystem stack SHALL mount and use MicroSD cards with capacity up to **64 GB** (SDHC/SDXC as applicable), including pre-formatted FAT32 and exFAT media the platform can read. Auto-format of a blank card SHALL produce a filesystem usable on that card up to 64 GB. Cards larger than 64 GB are out of scope for this change.

#### Scenario: 64 GB card mounts
- **WHEN** a usable 64 GB card is in the slot at boot
- **THEN** the host mounts it and can report capacity on the order of 64 GB (allowing normal filesystem overhead)

#### Scenario: Blank large card formats
- **WHEN** an unformatted card up to 64 GB is present and requires format
- **THEN** the host formats and mounts it successfully

### Requirement: Unmount frees mount RAM after SD save
After the host finishes an SD history save (periodic backup or pre-restart flush)—whether the write succeeded or failed after mount—the host SHALL unmount the SD filesystem and release mount/driver buffers so that RAM held for the mount is freed. The host SHALL NOT keep the card mounted between saves solely to hold the history path open. Boot probe/restore SHALL likewise unmount and free mount RAM when that session ends (after restore or after determining the card is usable/absent), except while a save is in progress.

#### Scenario: RAM freed after backup
- **WHEN** a periodic SD history backup completes
- **THEN** the SD filesystem is unmounted and mount RAM is released before idle operation continues

#### Scenario: Not left mounted between intervals
- **WHEN** SD history persist is active and the next backup is still minutes away
- **THEN** the card is not kept mounted only to await that backup

### Requirement: SD status exposes presence capacity and free space
After the P4 host has completed the boot SD probe, it SHALL expose SD **status** (at least: absent, ready/available when a usable card was found, error), **capacity** in whole megabytes, and **free** space in whole megabytes for Status and related APIs. Capacity and free SHALL reflect the last successful mount session (including values captured before unmount). When no usable card was found, status SHALL indicate absent (or equivalent) and MUST NOT claim a present card’s capacity.

#### Scenario: Ready card reports sizes after unmount
- **WHEN** a usable card was probed and Status is read while the card is unmounted between saves
- **THEN** status indicates the card is available/ready and capacity Mb and free Mb reflect the last mount readings

#### Scenario: Absent card status
- **WHEN** no usable card was found at probe
- **THEN** status indicates absent (or equivalent) and does not claim a mounted card

### Requirement: SD statistics come only from real I/O mounts
The host SHALL update cached SD capacity, free space, and on-card history snapshot size only during mount sessions that already perform probe, restore/read, or save/write. Status polling and other read-only UI/API paths MUST NOT remount the SD card solely to refresh those statistics.

#### Scenario: Status does not remount for free space
- **WHEN** Status is polled while the card is unmounted between saves
- **THEN** free Mb and capacity Mb come from cache and the host does not mount SD for that poll

#### Scenario: Cache refreshes on save
- **WHEN** a periodic SD history save mounts, writes, and unmounts
- **THEN** cached free space and on-SD history size are updated from that session before unmount
