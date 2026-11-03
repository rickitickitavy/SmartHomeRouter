# Spec Delta

## ADDED Requirements

### Requirement: Optimistic actuator feedback on toggle
In Place and simple-user view mode, toggling an actuator via switcher or icon MUST show the commanded state immediately, wait up to 5 seconds for confirming live status for that device/channel, and revert to the last known live (or pre-command) state if confirmation does not arrive.

#### Scenario: Immediate flip then confirm
- **WHEN** the operator turns an On/Off switcher from Off to On and a live ON status for that device/channel arrives within 5 seconds
- **THEN** the switcher shows On immediately on click and stays On after the status arrives

#### Scenario: Timeout reverts
- **WHEN** the operator turns an On/Off switcher from Off to On and no confirming ON status arrives within 5 seconds
- **THEN** the switcher reverts to Off (or the last known live state)

#### Scenario: Icon control same rule
- **WHEN** the operator toggles the same actuator via an icon control instead of a switcher
- **THEN** the icon color/state follows the same immediate flip, 5 second wait, and confirm-or-revert behavior

### Requirement: Group On/Off optimistic feedback is per member
Group On/Off MUST apply optimistic feedback independently per commanded On/Off member (immediate flip, ≤5s confirm-or-revert). One member’s timeout MUST NOT revert other members that already confirmed.

#### Scenario: Mixed group outcomes
- **WHEN** group On/Off commands two On/Off devices and only the first reports the new state within 5 seconds
- **THEN** the first stays on the commanded state and the second reverts after 5 seconds

#### Scenario: Second click replaces pending intent
- **WHEN** the operator toggles the same actuator again while a 5 second confirm wait is still open
- **THEN** the console cancels the previous wait for that device/channel, applies the new commanded state immediately, and starts a new 5 second confirm-or-revert wait for the latest command
