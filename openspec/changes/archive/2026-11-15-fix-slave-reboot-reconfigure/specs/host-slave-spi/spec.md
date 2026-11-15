## ADDED Requirements

### Requirement: Slave requests full config ten seconds after boot
After the slave SPI path is ready to talk to the host, the slave SHALL wait **10 seconds** from that boot-ready moment, then enqueue one slave-to-host **full configuration request** event on the same SPI event path as other slave→host frames. That request SHALL ask the host for the same configuration outcome the host applies on a normal boot configure (radio settings plus the full device and user store push). The slave SHALL NOT recover product settings or the device registry from its own NVS/LittleFS for this path. The 10-second wait SHALL apply on every slave boot, including abnormal reboot while the host remains up and joint boot with the host.

#### Scenario: Slave-only abnormal reboot while host is up
- **WHEN** the slave reboots while the host is already in normal SPI operation
- **THEN** about 10 seconds after the slave is SPI-ready it sends the full configuration request, and the host runs the normal configure protocol so the slave receives settings and the device/user stores again

#### Scenario: Joint boot — host still starting
- **WHEN** both chips boot and the host needs time before it can configure the slave
- **THEN** the slave still waits the 10-second pause before sending the full configuration request so the host has time to be ready to run configure

#### Scenario: Request is one-shot per boot
- **WHEN** the 10-second timer fires after a slave boot
- **THEN** the slave enqueues the full configuration request once for that boot (it SHALL NOT spam the request on every pump tick)

### Requirement: Host runs normal configure on slave full-config request
When the host receives the slave full configuration request (on S3 or P4), it SHALL run the same configure protocol used after a normal boot settings acceptance: enqueue `SET_SETTINGS` (or the existing settings push path), and after settings are accepted (`SettingsOk` / equivalent), republish host device and user stores to the slave via the existing `publishHostStores` path. The host SHALL NOT invent a separate registry-only channel for this request. The host SHALL accept this request while already in normal bring-up state, not only during initial `WaitReady`.

#### Scenario: Host already Normal when request arrives
- **WHEN** the host is in normal SPI operation and receives the slave full configuration request
- **THEN** it pushes settings and, after settings acceptance, publishes host stores to the slave as on normal boot configure

#### Scenario: Same path on S3 and P4
- **WHEN** either the ESP32-S3 host or the ESP32-P4 host receives the slave full configuration request
- **THEN** both run the same configure and store-publish behavior described above

#### Scenario: OTA slave update in progress
- **WHEN** a joined slave firmware update / OTA transfer is actively updating the slave
- **THEN** the host MAY defer or ignore the full configuration request until that update path is not actively transferring, so configure does not fight OTA (same caution as today’s READY re-push during slave update)
