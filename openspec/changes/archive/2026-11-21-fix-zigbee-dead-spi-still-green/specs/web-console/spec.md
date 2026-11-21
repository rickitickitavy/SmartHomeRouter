# Spec Delta: web-console

## ADDED Requirements

### Requirement: Hardware edits born interval
System → Hardware SHALL show and allow saving **BORN_INTERVAL_MIN** (minutes), using the same clamp rules as today (5–120, default 20). The control SHALL persist the existing `bornIntervalMin` setting (shared with MQTT born publish cadence when a topic is set). Only `isAdmin` SHALL edit it. GET/POST hardware (or the Hardware save path that already persists related gateway fields) SHALL round-trip this value; settings export/restore SHALL keep including it.

#### Scenario: Admin sets interval on Hardware
- **WHEN** an admin opens System → Hardware and saves born interval 30
- **THEN** `bornIntervalMin` is stored as 30 and Zigbee interval collect uses 30 minutes

## MODIFIED Requirements

### Requirement: MQTT card edits server-born topic and interval
The MQTT section SHALL show and allow saving **SERVER_BORN_TOPIC**. The MQTT section MUST NOT host the born-interval control after the move to Hardware (interval is edited under System → Hardware). GET `/api/mqtt` and POST `/api/mqtt` MAY still include `bornIntervalMin` for compatibility, but the operator-facing interval editor SHALL be on Hardware. Settings export and restore SHALL include both `serverBornTopic` and `bornIntervalMin`.

#### Scenario: MQTT shows topic only for born
- **WHEN** an admin opens MQTT
- **THEN** SERVER_BORN_TOPIC is editable there and BORN_INTERVAL_MIN is not edited on the MQTT card
