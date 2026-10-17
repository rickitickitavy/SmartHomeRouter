# Tasks

## 1. Persist and evaluate event type

- [x] 1.1 Add `eventType` on `AutomationListener` (`change`/`any`, default `change` on parse/omit); round-trip in save/list/export JSON — verify: omit loads as change; `"any"` survives reload
- [x] 1.2 Gate status-path listener matching: `change` requires value change; `any` allows same-value re-reports; drop blanket discrete always-notify for all listeners — verify: Change + same DRY does not evaluate; Any value + same DRY does
- [x] 1.3 When evaluation is admitted by an `any` listener, run actions if condition true without requiring false→true edge; keep current-time rising-edge; honor `minTriggerIntervalSec` — verify: Any value re-runs without reboot; Change still needs a real value change
- [x] 1.4 Bump `FIRMWARE_VERSION` build by 1 on apply (propose has `3.5.09`) — verify: `include/Defines.h`

## 2. Triggered-by-devices UI

- [x] 2.1 Add Event type select (**Change** / **Any value**, default Change) at the **end** of each listen row (after Field / custom cluster-attr, before Remove) — verify: new row defaults Change; order matches spec
- [x] 2.2 Put field labels on the same line as each listen control, including **Field** for the parameter picker — verify: Device, Channel, Field, Event type (and custom fields when shown) show in-line labels

## 3. Field soak

- [ ] 3.1 `alarm button` with Event type **Any value**: repeated DRY reports run again without reboot; with **Change**: same DRY does not re-run until value changes — verify: serial + `/api/status`
