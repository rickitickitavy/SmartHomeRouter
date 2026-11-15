# Design

## Context

Host owns product settings and the device/user registry (ADR 0003). Slave device store is RAM-only (`DeviceStore::begin(false)`). Normal bring-up:

1. Host resets slave → `WaitReady`
2. Slave one-shot `SpiEvtSlaveReady`
3. Host `SpiCmdSetSettings` → slave `SpiEvtSettingsOk`
4. Host event handler calls `ZIGBEE_SPI_PROXY.publishHostStores` (`SpiCmdSetDevice` / `SpiCmdSetUser` ENTRY frames)

Gap: in `HostBringupNormal`, a new `SLAVE_READY` only re-pushes settings when OTA-verify is active or `lastSettingsOkMs` is ≥ 8s old (`InterChipHost.cpp`). A slave-only reboot can miss that gate while keepalive still succeeds, so `publishHostStores` never runs again. Slave has no config-request surface today (`GetDevices` / dump paths are unused stubs).

Directed product behavior: slave waits **10s** after boot, then requests **ALL** config; host runs the **same** configure protocol as normal boot. Feature for **S3 and P4**.

## Goals / Non-Goals

**Goals**

- Recover empty slave registry/settings after abnormal slave reboot while host stays up.
- Keep one configure path: settings push → `SettingsOk` → `publishHostStores`.
- 10s slave delay so joint boot can finish host bring-up before the request.
- Same behavior on S3 and P4 hosts; shared slave firmware.

**Non-Goals**

- Persisting the device registry on the slave.
- Changing OTA SPI chunk/ACK protocol (only avoid fighting an active slave update).
- Replacing host-driven READY/settings bring-up; this request is a safety net, not a removal of READY.
- Pulling devices from slave to host (`GetDevices` dump direction).

## Decisions

1. **New SPI event: slave → host full-config request**  
   Add e.g. `SpiEvtConfigRequest` in `SpiProtocol.h` (next free event id). Slave enqueues it; host handles it like “please run configure.” Prefer a dedicated event over overloading `SLAVE_READY` so READY semantics and OTA-verify READY handling stay intact.

2. **Slave timer: 10s after SPI-ready, one-shot**  
   When the slave marks SPI ready (same moment it would send `SLAVE_READY`), start a 10s deadline. On expiry in `pump()` (or equivalent), enqueue `SpiEvtConfigRequest` once per boot (`configRequestSent` flag). Do not gate the request on “settings already received” — always request once so a host that ignored READY still reconfigures. A second configure on an already-healthy joint boot is acceptable and matches “request ALL config.”

3. **Host handler reuses existing chain**  
   On `SpiEvtConfigRequest` (when not mid slave OTA transfer): clear `settingsQueued` as needed, call `maybePushSettings()` (same as READY path). Existing `SpiEvtSettingsOk` handling already drives `publishHostStores` from `main.cpp` — keep that single republish hook. No new publish API.

4. **S3 and P4**  
   Shared `InterChipHost` / `ZigbeeSpiProxy` / `main` host path; both SoCs already share bring-up. No board-specific fork.

5. **OTA caution**  
   Mirror READY’s `!FIRMWARE_OTA.isUpdatingSlave()` guard so a config request during active slave OTA does not enqueue competing settings.

6. **Fragile zones**  
   Touches influence paths for `slave-spi-ota` / `firmware-update-process` (`SpiProtocol.h`, `InterChipHost.*`, `InterChipSlave.*`). Apply must get named zone approval before editing; do not change SPI begin/chunk/end OTA machinery.

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Extra full store push ~10s after every healthy boot | Acceptable per directed design; one-shot; same ENTRY frames as boot |
| Request arrives before host SPI Normal | Host may ignore until Normal, or queue until ready — prefer handle once Normal / after WaitReady can push settings; if host still resetting slave, READY path still owns first configure |
| Request during OTA | Ignore/defer while `isUpdatingSlave()` |
| Double READY + ConfigRequest | Idempotent settings + republish; OK |

## Migration Plan

- Flash host **and** slave (protocol event + both sides). Old host ignores unknown event; old slave never sends it — no partial-migrate benefit for the bugfix.
- No settings file / export schema change.

## Open Questions

- None for scope: 10s delay, full config request, same configure path, S3+P4 are fixed by the request. Exact event opcode is an implementation detail (pick next free `SpiEvt*`).
