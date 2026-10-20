# Spec Delta

## ADDED Requirements

### Requirement: Registered devices store custom commands
Each registered device record MUST persist an ordered list of custom commands (empty by default). Custom commands are settings: they MUST appear in device list JSON, single-device save/read, devices export/import, and after host reboot. Omitted `customCommands` on load MUST become an empty list. The slave MUST NOT be required to store or interpret custom commands.

#### Scenario: Survives reboot
- **WHEN** a device is saved with two custom commands and the host reboots
- **THEN** list JSON still includes those two commands with the same names, parameter flags, and types

#### Scenario: Legacy device without field
- **WHEN** the host loads a stored device that has no `customCommands` field
- **THEN** that record has an empty custom command list

#### Scenario: Export round-trip
- **WHEN** the operator exports devices and imports the same JSON
- **THEN** each device’s custom commands match what was exported
