## ADDED Requirements

### Requirement: Hardware sets S3 persist historical on internal flash
On the ESP32-S3 host console, System → Hardware SHALL show a checkbox **persist historical on internal flash** (square-only; default true). Persist MAY be ON only when Maximum history size is ≤ 4096 KiB. `/api/hardware` SHALL round-trip `historyPersistFlash`. Include in global settings export/restore. Omit on non-S3 hosts. Only `isAdmin`.

#### Scenario: Default checkbox on S3
- **WHEN** an S3 admin opens Hardware and persist was never saved and Maximum history size is ≤ 4096
- **THEN** persist historical on internal flash is checked

#### Scenario: Size above gate disables ON
- **WHEN** Maximum history size is 8192 and the admin tries to check persist historical on internal flash
- **THEN** the UI/API does not leave persist ON

#### Scenario: Save with large size clears persist
- **WHEN** persist is ON and the admin saves Maximum history size greater than 4096
- **THEN** after save, persist is OFF

### Requirement: Hardware sets S3 flash history save interval
On the ESP32-S3 host console, when persist historical on internal flash is ON, System → Hardware SHALL show **interval save historical data** (30–600 minutes, default 120). `/api/hardware` SHALL round-trip `historyFlashBackupIntervalMin` and reject out-of-range values. Hide or disable when persist is OFF. Omit on non-S3 hosts. Include in global settings export/restore. Only `isAdmin`.

#### Scenario: Default interval when persist ON
- **WHEN** an S3 admin opens Hardware with persist ON and the flash interval was never saved
- **THEN** the interval shows 120

#### Scenario: Persist OFF hides interval
- **WHEN** persist historical on internal flash is OFF
- **THEN** the interval save historical data field is not shown as an editable flash-persist control

#### Scenario: Out of range rejected
- **WHEN** the admin POSTs a flash backup interval of 10 minutes
- **THEN** the API rejects the value (below 30)

### Requirement: Status shows time before save for S3 flash persist
When S3 persist historical on internal flash is ON, Status SHALL show **time before save** from the status API countdown. When persist is OFF, Status SHALL NOT show a misleading flash-persist countdown.

#### Scenario: Persist ON shows countdown
- **WHEN** an admin opens Status and S3 flash persist is ON with a known remaining interval
- **THEN** Status displays time before save for the next flash history save

### Requirement: Status shows On-flash history size in Historical storage
When S3 persist historical on internal flash is ON, Status **Historical storage** SHALL show **On-flash history size** in kibibytes from the status API cached real LittleFS snapshot size (same role as P4 **on-SD history size**). When persist is OFF, that row SHALL NOT be shown. Operator Status and simple-user Status MUST both show this when the viewer can open Status and persist is ON.

#### Scenario: Persist ON shows On-flash size
- **WHEN** an operator opens Status, S3 flash persist is ON, and cached on-flash size is 200 KiB
- **THEN** Historical storage shows On-flash history size as 200 kB (or equivalent formatting)

#### Scenario: Persist OFF hides On-flash size
- **WHEN** S3 flash persist is OFF
- **THEN** Historical storage does not show On-flash history size

### Requirement: Status File system shows Historical data size on S3
On S3 hosts only, when persist historical on internal flash is ON, Status **File system** SHALL show **Historical data size** using the same cached on-flash history size as **On-flash history size** in Historical storage. When persist is OFF, that File system row SHALL NOT be shown. Non-S3 hosts SHALL NOT show this File system row. Operator and simple-user Status MUST match.

#### Scenario: S3 persist ON duplicates size under File system
- **WHEN** an S3 operator opens Status with flash persist ON and cached size 200 KiB
- **THEN** File system shows Historical data size matching On-flash history size

#### Scenario: Non-S3 omits File system historical size
- **WHEN** a P4 operator opens Status
- **THEN** File system does not show Historical data size (P4 keeps on-SD size only under Historical storage)
