# Tasks

## 1. Fix On/Off duplicate-hold polarity

- [x] 1.1 Change `ZigbeeCoordinator::suppressStatusReplacement` so that while the hold is active: same value as last held → return true (suppress); different value → update the hold and return false (publish); verify the old branch that suppressed opposite values is gone
- [x] 1.2 Align logging so `Kept channel status…` (or equivalent) fires only on duplicate-same suppress within the window; verify opposite ON→OFF does not log that line and does call `lightStateHandler` with `OFF`
- [x] 1.3 Remove or simplify unused `endpointCommandAfter` / `fromStatusRead` gate logic if it no longer affects suppress decisions after the polarity fix; verify both `handleAttributeReport` and `handleLightStateWithSource` still use the helper correctly

## 2. Host MQTT verification

- [x] 2.1 No host publish-path code change unless a gap is found; verify `onLightState` → `MqttClient::publishDeviceState` still publishes the forwarded `OFF` payload on the mapped state topic
- [x] 2.2 Bump `FIRMWARE_VERSION` build by 1 on apply (propose already set `3.4.16`); verify the string in `include/Defines.h` reflects the implementation build

## 3. Integration check

- [ ] 3.1 On device: toggle a registered Zigbee `onOff` switch ON/OFF several times within a few seconds; verify host logs show `MQTT send … data=ON` and `MQTT send … data=OFF` for each state **change**, and that back-to-back duplicate same-state reports within ~8s may be omitted

## Workflow follow-up

- Archive the change after review when implementation is complete.
