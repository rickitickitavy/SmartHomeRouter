# Design

## Context

See proposal.md — Why. Grounding:

- System → Internal DB was left visible on Repeater in `status-interconnect-history-ui`; role chrome must hide that tab again.
- Status Network already shows Zigbee/MQTT Tx/Rx; peer `packetsIn`/`packetsOut` exist on interconnect status JSON but are not summed into Status Network.
- Host NTP uses `configTime(0, 0, …)` (UTC). No persisted operator UTC offset yet. Interconnect already pushes Main-owned variables via `pushSharedVariablesToAllPeers` / `variableApplyHandler`.

## Goals / Non-Goals

**Goals:**

- Hide Internal DB on Repeater.
- Status Network totals for interconnect packets (summed Out=sent, In=received from Main’s view).
- Hardware UTC offset select (30-minute steps); Main pushes to Repeaters; Repeater non-editable.

**Non-Goals:**

- IANA zone database / DST rules.
- Per-peer packet rows on Status Network.
- Slave firmware changes.

## Decisions

### 1. Store offset as `int16_t` minutes from UTC

Persist `timeZoneUtcOffsetMin` in hardware reserved settings (or adjacent reserved slot). UI options: −12×60 … +14×60 in steps of 30. Apply with `configTime(offsetSec, 0, ntp…)`. Clamp/reject non-step values on POST.

**Alternatives:** IANA names — rejected (user chose UTC offset list).

### 2. Push key `timeZoneUtcOffsetMin` on interconnect variable path

Extend Main’s shared variable push (alongside checkAlive / channel / permit) to include the offset string. Repeater `variableApplyHandler` parses and calls SettingsManager + re-apply NTP. On Repeater Hardware GET, return the stored offset; UI marks control read-only or hides the select and shows a read-only value.

### 3. Status aggregates in `/api/status`

When building status on Main, sum `packetsOut` → `repeaterPacketsTx` (or equivalent) and `packetsIn` → `repeaterPacketsRx`. Both pages render two new Network rows. Same labels on operator and simple-user.

### 4. Internal DB tab gate

In Repeater role chrome (and admin visibility), `setHidden(internal-db tab, true)` when role is Repeater regardless of admin. If Internal DB was the active System tab when role switches, fall back to Log/Hardware.

## Risks / Trade-offs

- [Half-hour zones without DST may disagree with local civil time] → Accept; operator-chosen fixed offset only.
- [Offline Repeater misses push until next hello/variable] → Same as other Main-owned variables; just_booted + periodic push covers recovery.

## Migration Plan

1. Deploy host firmware + UI together.
2. Existing devices default to UTC+00:00 until Hardware save.
3. After Main save, Repeaters pick up offset on next variable push / boot hello.

## Open Questions

None — packet aggregation (2A) and UTC offset list with 30-minute steps (3A clarified) are fixed.
