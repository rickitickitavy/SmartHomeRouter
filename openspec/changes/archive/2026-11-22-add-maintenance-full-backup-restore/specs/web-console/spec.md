## ADDED Requirements

### Requirement: Maintenance Full backup and Full restore controls
On **Main** network role, System → Maintenance SHALL show **Full backup** and **Full restore from backup** above Export settings / Restore settings. Only `isAdmin` SHALL use them. On **Repeater**, those Full controls SHALL be hidden or disabled.

#### Scenario: Main shows Full controls at top
- **WHEN** an `isAdmin` on Main opens System → Maintenance
- **THEN** Full backup and Full restore from backup appear above Export settings

#### Scenario: Repeater hides Full controls
- **WHEN** network role is Repeater and an admin opens Maintenance
- **THEN** Full backup and Full restore from backup are not offered

### Requirement: Full backup ZIP with optional slices
Full backup SHALL download a single ZIP. The operator SHALL be able to toggle **Include Wi‑Fi**, **Include historical data**, and **Include important passwords**, each defaulting to **on**. The ZIP SHALL include the durable settings slices that Export settings already includes (devices, users, consoles, automations, cameras, scenes metadata, interconnect, MQTT/Zigbee/hardware/theme/e-mail non-secret fields, etc.). When scenes have bake images, those JPEG bytes SHALL be included in the ZIP whenever scenes are backed up (not optional). Ordinary Export settings behavior SHALL remain unchanged (still omit Wi‑Fi; still blank e-mail password; still omit bake bytes).

#### Scenario: Defaults on
- **WHEN** an admin opens Full backup options
- **THEN** Include Wi‑Fi, Include historical data, and Include important passwords are on

#### Scenario: Scene bake always packed
- **WHEN** a scene has a bake JPEG and Full backup runs
- **THEN** the ZIP contains that bake image for the scene

### Requirement: Important passwords encrypted in JSON
When **Include important passwords** is on, Full backup SHALL require an operator passphrase and SHALL store previously export-omitted secrets as **encrypted fields inside the JSON** (not as a password-encrypted ZIP). Those secrets SHALL include the e-mail SMTP password and, when Wi‑Fi is also included, the Wi‑Fi PSK. Secrets that Export settings already ships in plaintext (e.g. camera ONVIF passwords) SHALL NOT require this encryption wrapper. Console user passwords SHALL remain hashes only. When **Include important passwords** is off, the ZIP SHALL omit those important secret values (e-mail password blank/absent; Wi‑Fi PSK absent even if Wi‑Fi non-secret fields are included).

#### Scenario: Passphrase required for secrets
- **WHEN** Include important passwords is on and the admin starts Full backup without a passphrase
- **THEN** the backup does not download and the UI requires a passphrase

#### Scenario: Camera passwords stay plaintext in settings slice
- **WHEN** Full backup runs with cameras that have ONVIF passwords
- **THEN** those camera passwords remain in the settings slice as in Export settings (not re-wrapped by the important-password cipher)

### Requirement: Full restore wipe-then-apply
Full restore from backup SHALL, after the operator confirms and the archive is accepted for apply, **always remove** the Main host’s owned durable collections that the full-backup format replaces (at least devices, users, control consoles, automations, cameras, scenes and their bake images, and interconnect registry data as covered by settings restore) before applying the archive, so pre-existing rows/files cannot remain as orphans. After a successful apply the host SHALL restart.

#### Scenario: Wipe scenes removes bake files
- **WHEN** Full restore applies and the board had scene bake files on flash
- **THEN** those bake files are removed with the scene wipe before the backup’s scenes/images are applied

#### Scenario: Reboot after success
- **WHEN** Full restore finishes applying successfully
- **THEN** the host restarts

### Requirement: Full restore import choices for Wi‑Fi, e-mail, and history
Full restore SHALL let the operator choose whether to import **Wi‑Fi**, **e-mail**, and **historical data**. If Wi‑Fi is not imported or not present, the host SHALL keep its current Wi‑Fi settings (including PSK). If e-mail secrets are not imported or not decrypted, the host SHALL keep its current e-mail password (non-secret e-mail fields MAY still apply when present and e-mail import is on). If historical data is not imported or not present in the ZIP, the host SHALL **wipe** the history store to empty. If historical data is imported, the host SHALL replace history from the ZIP. Cross-board or older backups SHALL be allowed with the same bounded-numeric clamp rules as settings restore.

#### Scenario: Skip history wipes samples
- **WHEN** the operator restores a ZIP with history but turns off import historical data
- **THEN** after restore the history store is empty

#### Scenario: Skip Wi‑Fi keeps current
- **WHEN** the operator restores a ZIP that includes Wi‑Fi but turns off import Wi‑Fi
- **THEN** the board’s pre-restore Wi‑Fi settings remain

### Requirement: Wrong passphrase offers restore without important passwords
When Full restore needs a passphrase to decrypt important passwords and the passphrase is wrong, the host SHALL offer to continue the restore **without** applying important passwords (leave current e-mail/Wi‑Fi secrets) rather than only failing closed. The operator MAY cancel instead.

#### Scenario: Continue without secrets
- **WHEN** the passphrase is wrong and the operator chooses restore without important passwords
- **THEN** wipe-and-apply proceeds for non-secret slices and current e-mail/Wi‑Fi secrets are kept

### Requirement: Full restore STA failure uses SoftAP
After Full restore reboot, if Wi‑Fi MODE is STA and the join fails within the existing STA join window, the host SHALL switch to SoftAP recovery using the existing recovery-AP behavior while keeping the restored configuration.

#### Scenario: Bad STA after restore
- **WHEN** restored Wi‑Fi is STA to an unreachable network and the join window elapses
- **THEN** the host starts SoftAP recovery and does not roll back the restored config
