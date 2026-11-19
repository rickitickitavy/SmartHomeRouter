# Spec Delta: automations

## ADDED Requirements

### Requirement: Time change minute trigger
Each automation MUST persist a boolean Time change trigger (default false when omitted). When true and the automation is active, the host SHALL evaluate that automation once per local wall-clock minute at second 00 (host local time after Hardware UTC offset / NTP). That evaluation SHALL run actions when the condition tree is enabled and true (or when condition enable is false, treat condition as true), without requiring a device listen event. Min-trigger-interval, when set, still applies. Wall clock MUST be sane before a minute tick is honored.

#### Scenario: Fires at minute boundary
- **WHEN** an active automation has Time change true, condition enable true, a true time-of-day condition, and local time advances from 08:00:59 to 08:01:00
- **THEN** the host evaluates that automation on that minute tick and runs its actions

#### Scenario: Inactive skips tick
- **WHEN** an automation has Time change true but `active` is false
- **THEN** the host does not run its actions on the minute tick

#### Scenario: No sane clock skips tick
- **WHEN** host wall clock is not sane and Time change is true
- **THEN** the host does not treat that moment as a schedule tick

### Requirement: Time-of-day condition comparisons
A condition comparison MAY target Current time **time of day**. The compare value MUST be `HH:MM` (24-hour, local). Allowed compare ops are GREATER, EQUAL, and LESS. Evaluation MUST use host local time of day at minute resolution (seconds ignored / treated as 00). Legacy stored values `HH:MM:SS` MUST load by truncating to `HH:MM`.

#### Scenario: Equal at scheduled minute
- **WHEN** local time is 07:30 and a comparison is Current time time-of-day EQUAL `07:30`
- **THEN** that comparison is true

#### Scenario: Greater after threshold
- **WHEN** local time is 18:05 and a comparison is Current time time-of-day GREATER `18:00`
- **THEN** that comparison is true

#### Scenario: Legacy seconds truncated
- **WHEN** an older automation loads a Current time value `08:15:30`
- **THEN** the host treats it as `08:15` for persistence and comparison

### Requirement: Day-of-week condition comparisons
A condition comparison MAY target Current time **day of week**. The compare value MUST be an integer 1–7 where Monday=1 and Sunday=7. Allowed compare ops are GREATER, EQUAL, and LESS using that numeric order. Evaluation MUST use host local calendar day.

#### Scenario: Equal Monday
- **WHEN** local weekday is Monday and a comparison is day-of-week EQUAL `1`
- **THEN** that comparison is true

#### Scenario: Sunday is 7
- **WHEN** local weekday is Sunday and a comparison is day-of-week EQUAL `7`
- **THEN** that comparison is true

#### Scenario: Greater midweek
- **WHEN** local weekday is Wednesday (3) and a comparison is day-of-week GREATER `1`
- **THEN** that comparison is true

### Requirement: Schedule automation save validation
When Time change is true, the host MUST require condition enable true, at least one complete time-of-day or day-of-week comparison in the condition tree, and at least one action. Listening devices MAY be empty. The host MUST reject saves that violate this. When Time change is false, existing TIMER / listen / condition validation rules remain unchanged.

#### Scenario: Time change with time condition and action saves
- **WHEN** the operator saves an automation with Time change true, condition enable true, one time-of-day EQUAL condition, one action, and no listeners
- **THEN** the host accepts the save

#### Scenario: Time change without time condition rejected
- **WHEN** the operator saves an automation with Time change true, condition enable true, only a device status comparison, and actions present
- **THEN** the host rejects the save

#### Scenario: Time change without action rejected
- **WHEN** the operator saves an automation with Time change true and a valid time-of-day condition but no actions
- **THEN** the host rejects the save

## MODIFIED Requirements

### Requirement: Timer-only and non-timer validation
When TIMER `use` is false and Time change is false, the automation MUST have at least one listening device and, when condition enable is true, at least one condition; the host MUST reject saves that violate this. When TIMER `use` is true, or when condition enable is false, the automation MAY have no conditions; an empty or disabled condition MUST evaluate as true so the automation can run when triggered. When TIMER `use` is true, the automation MAY have no listening devices. When Time change is true, schedule automation save validation applies instead of the listen-required non-timer rule.

#### Scenario: Timer-only automation
- **WHEN** an active automation has TIMER `use` true, no listeners, no conditions, and the countdown finishes
- **THEN** the host treats conditions as true and runs its actions

#### Scenario: Non-timer requires listen and condition
- **WHEN** the operator saves an automation with TIMER `use` false, Time change false, condition enable true, and empty listeners
- **THEN** the host rejects the save

#### Scenario: Condition disabled without tree
- **WHEN** the operator saves an automation with condition enable false and an empty condition tree
- **THEN** the host accepts the save if other validation rules pass

#### Scenario: Time change uses schedule validation
- **WHEN** the operator saves with Time change true, TIMER `use` false, no listeners, a complete time-of-day condition, condition enable true, and one action
- **THEN** the host accepts the save under schedule validation
