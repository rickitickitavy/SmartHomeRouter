# Spec Delta: zigbee-slave-radio

## ADDED Requirements

### Requirement: Non-callback Zigbee APIs require the Zigbee lock
On the slave, any Zigbee SDK / ZBOSS API call made from a context that is **not** a Zigbee stack callback (including Arduino `loop`, deferred SPI→radio handlers, and Ping health) MUST be preceded by a successful `esp_zb_lock_acquire` with a finite timeout and MUST release the lock afterward. Zigbee stack callbacks that Espressif already runs on the Zigbee task MUST NOT re-acquire that lock. If acquire fails, the slave MUST NOT call the Zigbee API for that operation and MUST fail that operation soft (coordinator remains running unless a separate health rule trips).

#### Scenario: Neighbor walk under lock
- **WHEN** `dispatch` refreshes registered shorts or pairing neighbor shorts from the NWK neighbor table
- **THEN** those neighbor/address API calls run only while the Zigbee lock is held

#### Scenario: Failed lock skips ZDO
- **WHEN** a Simple Descriptor or Active Endpoints request cannot acquire the Zigbee lock before the timeout
- **THEN** the slave does not invoke the ZDO request API unlocked and does not treat that probe as sent

#### Scenario: ZCL send still timed
- **WHEN** the slave sends a ZCL packet from a non-callback path
- **THEN** it uses a finite lock timeout and releases the lock after the send attempt

### Requirement: Zigbee health proves lock and buffer pool
While the slave reports Zigbee as started, Ping-time Zigbee health MUST succeed only when the Zigbee lock can be acquired within a finite timeout **and**, under that same lock hold, `zb_buf_get_out` returns a usable buffer that the slave then frees before releasing the lock. A failed lock or a zero buffer id MUST fail health.

#### Scenario: Buffer pool empty fails health
- **WHEN** Ping runs health, the lock is acquired, and `zb_buf_get_out` returns 0
- **THEN** health fails and the slave treats Zigbee as lost for that Ping (clear started / schedule reboot per existing lost-Zigbee path)

#### Scenario: Lock free but stack usable
- **WHEN** Ping runs health, the lock is acquired, and a buffer is obtained and freed
- **THEN** health succeeds

### Requirement: Consecutive radio send failures trip Zigbee lost
After Zigbee has been started for at least a short grace period (design: 60 s), the slave MUST count consecutive radio send failures caused by Zigbee lock miss or no outbound Zigbee buffer. When that streak reaches the design threshold (design: 5), the slave MUST mark Zigbee unhealthy using the same lost-Zigbee reboot path as a failed Ping health check. A successful radio send MUST reset the streak to zero. The slave MUST NOT mark Zigbee lost solely because no inbound device reports arrived for a long quiet period.

#### Scenario: Streak of no-buffer sends
- **WHEN** grace has elapsed and five consecutive ZCL (or equivalent) sends fail with no Zigbee buffer
- **THEN** the slave schedules the Zigbee-lost reboot path

#### Scenario: Success clears streak
- **WHEN** two sends failed for lock/buffer and the next send succeeds
- **THEN** the consecutive failure count returns to zero

#### Scenario: Quiet mesh does not force lost
- **WHEN** Zigbee has been started for hours with no inbound reports and no failed send streak
- **THEN** the slave does not mark Zigbee lost for quietness alone
