# Design

## Context

See proposal.md — Why. Intended rule for `StatusAnswerHold` (`kStatusReadGiveUpMs` = 8s): suppress a report only when its value equals the **last published** value for that IEEE + endpoint + cluster and the hold has not expired. A different value must publish and become the new last published.

Observed bug: the comparison polarity is inverted. Today `hold->value == value` returns false (allow duplicate), and a different value hits `Kept channel status…` and returns true (suppress change). That matches the field log: repeated ON still MQTT-publishes; OFF within 8s is dropped.

## Goals / Non-Goals

**Goals:**

- Implement duplicate-only suppress: same as last within the window → do not forward; different from last → forward and update the hold.
- Restore OFF (and any opposite) publishes after rapid toggles.

**Non-Goals:**

- Changing MQTT topic mapping, SPI frame layout, or boot status-read queueing.
- Changing outbound one-in-flight command coalescing (`fix-zigbee-on-off-command-echo`).
- Fixing unrelated `Status read timeout` noise beyond what falls out of hold logic.

## Decisions

1. **Invert the suppress condition to match the intended rule.** While a hold is active for IEEE/endpoint/cluster: if `value == hold->value`, return true (suppress duplicate). If `value != hold->value`, update `hold->value` / timestamps and return false (publish change). On expiry, replace or recreate the hold and publish. Alternative: remove the hold entirely. Rejected — duplicate flood within 8s is still worth filtering.

2. **Drop opposite-value / command-stamp special cases as the primary gate.** `endpointCommandAfter` and `fromStatusRead` were compensating around the wrong polarity. After the fix, opposite reports no longer need a command stamp to get through. Keep or simplify those helpers only if something else still needs them; do not reintroduce “suppress different value” behavior.

3. **Host path stays as-is.** Once the slave calls `lightStateHandler` with `OFF`, SPI → `onLightState` → `publishDeviceState` already publishes.

4. **Logging.** Reserve `Kept channel status…` for the duplicate-same suppress path (or rename later if the wording confuses operators).

## Risks / Trade-offs

- [Repeated identical reports within 8s are silent] → Intended; MQTT and console keep the last published state.
- [Hold window still 8s] → Keep `kStatusReadGiveUpMs`; only polarity changes unless a later change asks to tune it.

## Migration Plan

Flash joined firmware (slave + host). No settings or device-list migration. Rollback is the previous image. After flash, toggle a Zigbee switch several times and confirm host logs `MQTT send … data=OFF` for each OFF that differs from the last publish.

## Open Questions

None.
