# Spec Delta: zigbee-slave-radio

## ADDED Requirements

### Requirement: Deferred radio apply failure trips Zigbee lost immediately
**The slave knows when RF / radio-apply is down.** When the slave has deferred radio work pending (permit-join, attribute read/write, cluster command, rebind, or equivalent device-control radio work) and that work cannot be applied (Zigbee coordinator not started when required, Zigbee lock not acquired in time for a required stack call, Search/permit-join cannot arm pairing blue blink / open join, or equivalent hard apply failure), the slave MUST mark Zigbee lost using the existing lost-Zigbee path (clear Zigbee-started for Ping) and MUST **reboot itself immediately**. Finding no Zigbee devices or receiving no end-device replies MUST NOT by itself be treated as Zigbee failure. The slave MUST NOT add a periodic Zigbee self-test / fake probe (including a once-per-minute test without real permit-join). The slave MUST NOT trip this solely during an intentional firmware-transfer quiet pause of deferred radio.

#### Scenario: No standing self-test probe
- **WHEN** the slave is running normally with no deferred apply failure
- **THEN** it does not run a periodic fake Zigbee self-test that is separate from real deferred radio work / server-born collect apply

#### Scenario: Lock miss on pending radio work
- **WHEN** deferred radio work is pending and apply cannot acquire the Zigbee lock before the timeout
- **THEN** the slave marks Zigbee lost and reboots immediately

#### Scenario: Coordinator down with pending permit-join
- **WHEN** deferred permit-join is pending and the coordinator is not started
- **THEN** the slave marks Zigbee lost and reboots immediately

#### Scenario: No devices found is not failure
- **WHEN** status collect / network work runs but no known Zigbee devices answer or none are found
- **THEN** the slave does not mark Zigbee lost for that empty result alone

#### Scenario: Quiet end devices do not trip lost
- **WHEN** status reads were applied on the air path but sleepy or offline devices do not reply
- **THEN** the slave does not mark Zigbee lost for missing end-device replies alone

### Requirement: Permit-join success only after deferred apply
When the slave receives a host permit-join command (Search open or close), it MUST NOT signal command success to the host at the moment the command is only queued as deferred work. It MUST signal success only after deferred apply has completed the open or close network path. For a non-zero open window, that apply MUST include arming the existing pairing / blue blink hold used for Search. If apply cannot complete, the slave MUST signal command **failure** to the host and MUST follow the deferred-apply Zigbee-lost rule above.

#### Scenario: Search open arms blink then succeeds
- **WHEN** the host sends permit-join with a non-zero window and deferred apply runs successfully
- **THEN** the slave arms pairing/blue blink, opens the join window, and only then reports permit-join success

#### Scenario: Apply failure reports error
- **WHEN** deferred permit-join apply cannot run
- **THEN** the slave reports permit-join failure to the host and marks Zigbee lost per the rule above

### Requirement: Permit-join open/close uses Zigbee lock
When the slave applies deferred permit-join open or close from a non-callback context, it MUST acquire the Zigbee lock with a finite timeout before calling the stack open/close network API. If acquire fails, it MUST NOT call that API unlocked and MUST treat the apply as failed.

#### Scenario: Lock miss fails apply
- **WHEN** deferred permit-join cannot acquire the Zigbee lock before the timeout
- **THEN** the slave does not call open/close network unlocked and treats apply as failed
