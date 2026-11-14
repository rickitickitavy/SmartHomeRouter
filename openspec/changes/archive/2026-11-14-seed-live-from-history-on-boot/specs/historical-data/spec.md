# Spec Delta

## ADDED Requirements

### Requirement: Seed live cache from last history sample after boot restore
After the host restores a valid history snapshot into PSRAM (LittleFS or SD) and has synced historized series to the device map, for each series that has a last stored sample the host SHALL copy that sample into the host runtime live-value cache used by device-list and console APIs. Seeding SHALL NOT append a new history record. Seeding SHALL NOT run when no last sample exists for that series.

#### Scenario: Reportable series seeds console live value
- **WHEN** boot restores a snapshot whose temperature reportable series has a last sample `21.5` and Place shows that binding
- **THEN** the live reading shows `21.5` (formatted as today) before any new Zigbee/MQTT report for that attribute

#### Scenario: Empty series stays absent
- **WHEN** a historized series exists after restore but has no stored samples
- **THEN** the live cache for that attribute remains empty until a real report arrives

#### Scenario: Seed does not append history
- **WHEN** the host seeds live cache from a restored last sample
- **THEN** the series sample count does not increase solely because of that seed

### Requirement: History seed is not a status trigger
Applying a history-seeded live value SHALL NOT be treated as a device status-change event for automations or other status-path listeners. A later real report SHALL update the live cache and follow normal history append and automation rules.

#### Scenario: Seed does not fire automations
- **WHEN** an active automation listens for a change on an attribute that was seeded from history after reboot
- **THEN** that seed alone does not evaluate or enqueue the automation’s actions
