# Design: S3 internal-flash history persist

## Context

Today LittleFS history files (`/littlefs/history.bin` + meta) are written on reboot (or reboot-path flush) and **deleted after a successful boot restore**. P4 MicroSD mode keeps snapshots on the card and supports periodic backup via `historySdBackupIntervalMin` (5–600, default 60). S3 boards have no SD path for history and need durable flash persist with operator-controlled size and interval limits.

## Goals / Non-Goals

**Goals**

- Hardware setting **persist historical on internal flash** on S3 (default **true**).
- Persist may be ON only if **Maximum history size ≤ 4096 KiB**; raising size above that auto-turns persist OFF on the controller.
- When persist is ON: periodic interval save (30–600 min, default **120**), and **keep** LittleFS snapshot files after boot restore.
- When persist is ON: Status shows cached **On-flash history size** in Historical storage and the same value as **Historical data size** under File system (S3 only).
- When persist is OFF: keep existing reboot-path LittleFS flush + **delete-after-restore** behavior (no durable periodic flash persist).

**Non-Goals**

- Changing P4 SD backup semantics or SD card UI.
- Raising the 4096 KiB gate or changing default Maximum history size product-wide.
- Slave changes.

## Decisions

### Persist gate and auto-OFF

- Setting key (suggested): `historyPersistFlash` (bool), S3-relevant; ignored/hidden on P4 SD path as appropriate.
- UI and API: enabling persist is rejected (or forced false) when `historyMaxSizeKiB > 4096`.
- Whenever `historyMaxSizeKiB` is set **> 4096**, host sets `historyPersistFlash = false` and persists hardware settings (even if the client did not send persist=false).

### Interval when persist ON

- Suggested key: `historyFlashBackupIntervalMin` (or reuse a shared name only if it does not collide with P4 SD field semantics).
- Clamp on S3: **30–600**, default **120**.
- Periodic save writes the same LittleFS snapshot format used today; do not delete after write.
- Status “time before save” SHOULD surface when persist is ON (mirror P4 SD countdown pattern for flash).

### On-flash history size (Status)

- Cache real LittleFS snapshot file size (KiB) from mount/read/write sessions that already touch the snapshot (boot restore or periodic/reboot save) — same pattern as P4 on-SD size (no remount solely for Status).
- When persist is ON, Status **Historical storage** shows **On-flash history size** (label differs from P4’s **on-SD history size**; same semantics).
- On S3 only, Status **File system** also shows the same cached KiB as **Historical data size** (duplicate of the Historical storage value).
- When persist is OFF, hide both rows (or show no misleading size).

### Boot restore when persist ON

- Load snapshot into RAM as today.
- **Do not** call delete-after-restore for LittleFS history files when `historyPersistFlash` is true.
- When persist is OFF, keep current delete-after-successful-restore.

### UI

- Hardware History card (S3): checkbox (square-only click) + interval field visible when persist is ON.
- Max size field: if operator raises above 4096 while persist ON, UI reflects auto-OFF after save/reload (server is source of truth).
- Status: Historical storage **On-flash history size**; File system **Historical data size** (S3 only); both from the same cached KiB when persist ON.

## Risks / Trade-offs

- Flash wear: mitigated by min interval 30 min and 4096 KiB cap for persist mode.
- Full LittleFS partition: persist ON keeps larger durable files; operator must keep max size ≤ 4096 when using persist.
- Default true: S3 devices start with persist unless size is raised above 4096 (then auto-OFF).

## Migration

- Missing `historyPersistFlash` → **true** on S3; missing interval → **120**.
- Existing devices with `historyMaxSizeKiB > 4096` → persist forced **false** on load/save of hardware settings.
- Global settings export/restore already includes Hardware; add new fields to that slice.

## Open Questions

- None blocking; Status countdown and on-flash size (plus File system duplicate) are in-scope as P4-parity UX.
