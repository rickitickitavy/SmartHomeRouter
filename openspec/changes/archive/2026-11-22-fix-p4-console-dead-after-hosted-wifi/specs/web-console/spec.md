## MODIFIED Requirements

### Requirement: Console stays reachable after STA address refresh
When the host is in STA mode and the station obtains an IPv4 address after a disconnect, roam, or DHCP renew, the HTTP web console on port 80 MUST answer on that current STA address. ICMP reachability and MQTT client traffic MUST NOT be treated as proof that the console is already bound. The first HTTP bind after a successful boot join MUST remain the bind used until a later STA address event; that first bind MUST NOT be torn down immediately. USB CLI, MQTT, and Zigbee/SPI control MUST keep working while the console listen socket is rebound.

On **ESP32-P4** hosts (Wi‑Fi via ESP-Hosted C6), the same reachability rule MUST also hold after a Hosted transport or remote Wi‑Fi stack recovery that leaves STA usable again even when classic Arduino STA disconnect / lost-IP / GOT_IP edges (and a polled `WL_CONNECTED` drop) did not fire. That Hosted recovery MUST arm the same console listen rebind path used after STA GOT_IP, and the rebind MUST run from the normal host service loop (not from a Wi‑Fi/Hosted event task). ESP32-S3 host behavior for this requirement MUST remain unchanged by this change.

#### Scenario: Console answers after STA reconnect
- **WHEN** STA drops and later has an IPv4 address again
- **THEN** opening `http://<sta-ip>/` returns the console (login or signed-in) without requiring a host reboot

#### Scenario: Console answers after STA GOT_IP without a polled disconnect
- **WHEN** STA reports a new or renewed IPv4 address while ICMP and MQTT still work
- **THEN** the console answers on that STA address after the listen socket is rebound

#### Scenario: First bind after boot is not torn down immediately
- **WHEN** the host has just bound the console after the boot STA join succeeded
- **THEN** that first bind is not immediately ended solely because of the boot GOT_IP

#### Scenario: P4 Hosted recovery without classic STA edges
- **WHEN** the ESP32-P4 host is in STA mode, Hosted/C6 Wi‑Fi or transport recovers while ICMP and MQTT still work, and no STA disconnect/GOT_IP (and no polled disconnect edge) armed rebind
- **THEN** the host still arms and runs the console listen rebind so `http://<sta-ip>/` answers again without a host reboot

#### Scenario: S3 unchanged
- **WHEN** an ESP32-S3 host recovers STA using the existing disconnect/GOT_IP/polled path
- **THEN** console rebind behavior remains the pre-change S3 path (no Hosted-specific arm required)
