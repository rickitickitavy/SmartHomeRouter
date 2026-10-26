# Tasks

## 1. Firmware version

- [x] 1.1 Bump `FIRMWARE_VERSION` build by 1 on apply (propose already set `3.5.68`) and verify `include/Defines.h` shows the apply build

## 2. Raise deferred queue

- [x] 2.1 Set `InterChipSlave::kMaxDeferredDeviceCommands` from `32` to `128` in `include/InterChipSlave.h` — verify the constant is `128` and the full-table warning path still compiles
- [x] 2.2 Smoke: burst device-control to many distinct IEEE+endpoint destinations without `"Deferred device command table full"` until beyond 128 distinct destinations — verify the 129th new destination is rejected while replace on an existing slot still works

## Workflow follow-up

- Archive the change after review and apply are complete.
