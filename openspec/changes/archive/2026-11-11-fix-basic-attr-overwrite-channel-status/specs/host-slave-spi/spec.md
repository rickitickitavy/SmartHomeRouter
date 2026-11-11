## MODIFIED Requirements

### Requirement: Attribute reports update last status and battery
A slave-to-host attribute or zone report SHALL continue to include IEEE, source endpoint, short address, RSSI, and message text. The host SHALL cache the message against that IEEE and `ep` as last status, except when the message is a battery report (`BATTERY <percent>` from cluster `0x0001` attribute Battery Percentage Remaining) or a Basic-cluster (`0x0000`) report (including ApplicationVersion `attr=0x0001` and other Basic identity/metadata attributes delivered as `cl=/attr=/val=`). A battery report SHALL update last battery percent for that IEEE and SHALL NOT replace per-endpoint on/off or zone status. A Basic report SHALL NOT replace per-endpoint channel status. When the host receives a converted `cl=/attr=/val=` report whose text is not a discrete channel token (`ON`, `OFF`, `LEAK`, `DRY`, `UP`, `DOWN`, `STOPPED`) and that endpoint already holds a discrete token, the host SHALL keep the existing discrete status (and MAY still cache reportable values). The slave SHALL NOT persist last status or battery.

#### Scenario: On/off report caches endpoint
- **WHEN** the slave reports `ON` from endpoint 3
- **THEN** the host last status for that IEEE endpoint 3 is `ON`

#### Scenario: Battery does not overwrite channel state
- **WHEN** endpoint 1 last status is `OFF` and a later battery report arrives for that IEEE
- **THEN** last battery is the decoded percent and endpoint 1 status remains `OFF`

#### Scenario: Basic ApplicationVersion does not overwrite channel state
- **WHEN** endpoint 1 last status is `ON` and a later Basic cluster ApplicationVersion (`attr=0x0001`) report arrives for that IEEE
- **THEN** endpoint 1 status remains `ON` (ApplicationVersion is identity metadata, not channel state)

#### Scenario: Converted cl= measurement does not wipe On/Off
- **WHEN** endpoint 1 last status is `ON` and a later non-OnOff `cl=/attr=/val=` report converts to a numeric string for that endpoint
- **THEN** endpoint 1 status remains `ON`
