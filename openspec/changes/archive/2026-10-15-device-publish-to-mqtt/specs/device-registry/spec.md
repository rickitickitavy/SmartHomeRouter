# Spec Delta

## ADDED Requirements

### Requirement: Non-MQTT devices store publishToMqtt
Each registered device whose transport is not `mqtt` SHALL store a boolean `publishToMqtt`. The field SHALL appear in `devices.json`, device list JSON after reboot, and device create/update APIs. MQTT-transport devices SHALL NOT store or require this field. When a stored non-MQTT record omits `publishToMqtt`, the host SHALL treat it as `true`. When the host creates a new non-MQTT device record, `publishToMqtt` SHALL default to `false` unless the save body explicitly sets it.

#### Scenario: Legacy zigbee without field
- **WHEN** the host loads a `zigbee` device from `devices.json` with no `publishToMqtt` key
- **THEN** list JSON reports `publishToMqtt` as true

#### Scenario: New zigbee defaults off
- **WHEN** the operator saves a new `zigbee` device and the body omits `publishToMqtt`
- **THEN** the stored record has `publishToMqtt` false

#### Scenario: Explicit save
- **WHEN** the operator saves a non-MQTT device with `publishToMqtt` true
- **THEN** after reload the record still has `publishToMqtt` true

#### Scenario: MQTT transport excluded
- **WHEN** the operator saves or loads an `mqtt` transport device
- **THEN** the record does not require `publishToMqtt` and MQTT-native behavior is unchanged
