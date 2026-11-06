# Spec Delta

## MODIFIED Requirements

### Requirement: Users table fields
The host MUST persist a users table. Each user MUST have a unique user name, a password hash (never a plaintext password), an added-at timestamp, a list of roles, `isBlocked`, a theme (`light` or `dark`), and `consoles` (zero or more control-console ids). Roles are `isAdmin`, `editDevices`, `addDevices`, `removeDevices`, `editUsers`, `editConsoles`, `editScenes`, and `controlDevices` (Other device control). A user with none of those roles MUST be treated as a simple user. `isAdmin` MUST grant every console right, including `editConsoles`, `editScenes`, and `controlDevices`, even when those flags are false on that record. The persisted file MUST NOT store the password in clear text.

#### Scenario: Persist and reload a user
- **WHEN** an `isAdmin` operator creates user `alice` with role `editDevices`, theme `dark`, and a password
- **THEN** after reboot the host still has `alice` with `editDevices`, theme `dark`, the same added-at, and a stored hash that is not the typed password

#### Scenario: Simple user has no extra roles
- **WHEN** a user exists with no roles set
- **THEN** that user is a simple user and does not receive `isAdmin` or the extra roles

#### Scenario: Persist consoles assignment
- **WHEN** an operator saves user `bob` with `consoles` containing `Kitchen` and `Hall`
- **THEN** after reboot `bob` still has those two consoles assigned

#### Scenario: Persist Console editor role
- **WHEN** an operator saves user `eve` with `editConsoles`
- **THEN** after reboot `eve` still has `editConsoles`

#### Scenario: Admin has Console editor without the flag
- **WHEN** user `admin` has `isAdmin` and `editConsoles` is false
- **THEN** that user may still create and save control consoles

#### Scenario: Persist Other device control
- **WHEN** an operator saves user `cara` with `controlDevices`
- **THEN** after reboot `cara` still has `controlDevices`

#### Scenario: Persist editScenes role
- **WHEN** an operator saves user `sam` with `editScenes`
- **THEN** after reboot `sam` still has `editScenes`

#### Scenario: Admin can edit scenes without the flag
- **WHEN** user `admin` has `isAdmin` and `editScenes` is false
- **THEN** that user may still create and save scenes

## ADDED Requirements

### Requirement: editScenes round-trips with user sync
`editScenes` MUST round-trip through single-user save, the users list, settings export/restore, and reboot. It is settings, not live telemetry. Host↔slave user sync MUST carry `editScenes` when user frames include role flags.

#### Scenario: List JSON includes editScenes
- **WHEN** `sam` is saved with `editScenes`
- **THEN** the users list JSON for `sam` includes `editScenes` true
