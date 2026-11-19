# Tasks

## 1. Repeater Internal DB visibility

- [x] 1.1 Hide System → Internal DB when network role is Repeater (role chrome + admin tab gates); verify the tab is absent on Repeater and still available on Main for admin

## 2. Status Network repeater packet totals

- [x] 2.1 Expose aggregated repeater packets sent/received on the gateway status API (sum of peer packetsOut / packetsIn on Main; 0 otherwise); verify status JSON values match peer sums
- [x] 2.2 Add **Repeater packets sent** and **Repeater packets received** rows to Status Network on `data/index.html` and `data/user.html`; verify both pages show the same labels and values

## 3. Hardware UTC time zone

- [x] 3.1 Add persisted `timeZoneUtcOffsetMin` (30-minute-step list UTC−12:00…UTC+14:00), Hardware select on Main, `/api/hardware` round-trip, apply via NTP/`configTime`, and include in settings export/restore; verify save/load and clock offset apply
- [x] 3.2 On Repeater, show Time zone read-only (or hidden with effective value only) and reject independent edits; verify Repeater cannot change offset locally
- [x] 3.3 Main pushes offset to paired Repeaters on save and on shared variable / just_booted refresh; verify a Repeater applies Main’s offset after save or re-hello

## 4. Integration check

- [x] 4.1 Smoke-check Main vs Repeater: Internal DB hidden only on Repeater; Status Network shows repeater totals on both Status pages; Hardware time zone editable on Main and Main-owned on Repeater
