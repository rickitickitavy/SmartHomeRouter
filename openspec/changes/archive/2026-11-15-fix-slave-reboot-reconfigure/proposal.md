# Proposal

## Why

When the slave reboots abnormally (panic, watchdog, error) while the host/router keeps running, the host often does **not** re-run the full configure path. The slave boots with an empty RAM device registry and no radio settings from the host, so Zigbee devices and other pushed config are missing until something else forces a host reset of the slave.

Today configuration is host-driven: `SLAVE_READY` → `SET_SETTINGS` → `SettingsOk` → `publishHostStores` (devices/users). In `HostBringupNormal`, a late `SLAVE_READY` only re-pushes settings when OTA-verify is active or the last `SettingsOk` is at least **8s** old. A fast slave-only reboot can therefore leave the link “healthy” (keepalive) while the slave never gets a full registry push. The slave also has no live “please configure me” request.

## What Changes

- After every slave boot, the slave waits **10 seconds**, then requests **full configuration** from the host (same outcome as normal router boot configure: settings + device/user stores).
- The **10s** delay covers the normal joint-boot case where the host may still be coming up before it can configure the slave.
- On that request, the host (S3 and P4) runs the **same** configure protocol as after a normal boot `SettingsOk` path: push settings, then republish host stores to the slave.
- Applies to both host SoCs (**S3** and **P4**); slave firmware change is shared.

## Capabilities

### New Capabilities
- _(none)_

### Modified Capabilities
- `host-slave-spi`: slave post-boot full-config request after 10s; host handles it with the normal configure / store-publish path on S3 and P4.

## Impact

- Slave: boot timer → one SPI “config request” event; registry remains host-owned / RAM on slave.
- Host (S3/P4): handle config request by re-entering the existing settings + `publishHostStores` chain (not a separate registry channel).
- SPI protocol: new slave→host event (or equivalent) for the request; InterChip host/slave bring-up paths.
- Fragile zones **`slave-spi-ota`** / **`firmware-update-process`** influence InterChip/SpiProtocol — implementation needs named zone approval before editing those files.
- Propose bumps: host **4.3.04**, slave **4.2.10**.
