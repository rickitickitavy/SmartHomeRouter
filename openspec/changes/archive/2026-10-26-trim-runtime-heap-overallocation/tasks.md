# Tasks

## 1. Firmware version

- [x] 1.1 Bump `FIRMWARE_VERSION` build by 1 on apply (propose already set `3.5.70`) and verify `include/Defines.h` shows the apply build

## 2. EndpointStatus exact allocation

- [x] 2.1 Replace `EndpointStatus` fixed `state[384]` and covering arrays with `char *state` plus optional `CoveringStatus *` (or equivalent) in `ZigbeeSpiProxy.h`; verify `sizeof(EndpointStatus)` is far below 425 on a host build/compile check
- [x] 2.2 Add set/clear/ensure helpers and update all `ZigbeeSpiProxy.cpp` readers/writers (`noteReport…`, status getters, JSON helpers) to use pointers; verify a short `ON`/`OFF` update does not allocate a 384-byte buffer (log size or unit-style assert in helper)
- [x] 2.3 Confirm `channelCapacityForIeee` / `ensureChannelCapacity` size to configured channel count (not `DEVICE_CHANNEL_COUNT_MAX` when count is known); verify with a 1-channel and 4-channel registry entry that `channelCapacity` matches
- [x] 2.4 Free state and covering extras in `releaseSlot` and any clear paths; verify no leak on device leave/re-alloc (asan or repeated alloc/free smoke)

## 3. CachedDevice manufacturer / model

- [x] 3.1 Change `manufacturer`/`model` to `char *`, allocate on SPI join with bounded duplicate, free on release; verify MQTT-only slots leave both nullptr
- [x] 3.2 Update `devicesJson` / join handling call sites; verify bound-device JSON still includes manufacturer and model after a join that supplied them

## 4. ConsoleBinding optional side records

- [x] 4.1 Introduce `ConsoleGaugeConfig` / `ConsoleIconConfig` / `ConsoleLogicalConfig` (or equivalent) and shrink `ConsoleBinding` in `ConsoleStore.h`; verify non-gauge bindings do not allocate a gauge record after parse
- [x] 4.2 Update parse, free, and serialize paths in `ConsoleStore.cpp` so gauge/icon/logical JSON round-trips unchanged; verify load+save of a console that mixes default, gauge, and icon bindings matches prior JSON fields
- [x] 4.3 Grep and fix any host call sites that read inline `gaugeMin`/`hasGauge`/`iconScale`/`logicalThreshold`; verify project builds for the host env

## 5. Integration check

- [x] 5.1 Flash/load `settings (11).json` (or equivalent) and smoke: consoles render gauges/icons, automations still see device status, Devices table still shows manu/model for joined Zigbee; verify free heap is materially higher than pre-change with the same config
