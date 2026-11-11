## 1. Slave Basic handling

- [x] 1.1 In `ZigbeeCoordinator::handleAttributeReport`, early-return for the entire Basic cluster after log/pulse; keep ManufacturerName / ModelIdentifier identity apply
- [x] 1.2 Confirm On/Off, battery `BATTERY %u`, IAS, covering, and other `cl=` measurement forwards are unchanged

## 2. Host status cache

- [x] 2.1 In `ZigbeeSpiProxy::noteReportTelemetry`, do not write Basic `cl=` reports into per-endpoint channel status
- [x] 2.2 Treat Power Config battery delivered as `cl=` like `BATTERY %u` (update battery only; do not replace channel status)
- [x] 2.3 When cached status is discrete (`ON`/`OFF`/`LEAK`/`DRY`/move tokens) and a converted `cl=` message is not discrete, keep the discrete status (still allow reportable notify)

## 3. Specs and versions

- [x] 3.1 Sync main `host-slave-spi` requirement + Basic / discrete-lock scenarios from the delta
- [x] 3.2 Sync main `zigbee-slave-radio` Basic off status-path requirement from the delta
- [x] 3.3 Bump `FIRMWARE_VERSION_HOST` and `FIRMWARE_VERSION_SLAVE` builds for this change (`4.2.20` / `4.2.04`)

## 4. Verify

- [x] 4.1 Implementation review: slave Basic early-return and host status guards are in tree; kitchen Basic ApplicationVersion can no longer replace `ON` in channel status (operator flash confirm after deploy)
- [x] 4.2 Implementation review: battery/`cl=` measurement paths update reportable/battery without discrete overwrite guards regressing On/Off
