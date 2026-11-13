# Tasks

## 1. SPI rebind command and result

- [x] 1.1 Add host→slave rebind command and slave→host rebind result framing in `SpiProtocol` (IEEE + outcome) and verify pack/unpack lengths fit `SPI_MAX_PAYLOAD`
- [x] 1.2 Wire host enqueue + slave dispatch for the new command without removing the IEEE from the registered map — verify: registered count unchanged after a rebind frame is handled

## 2. Slave immediate bind and pairing fallback

- [x] 2.1 Implement immediate ZDO bind for a registered IEEE using usable short and endpoints for **all** Zigbee kinds on the shared path; on success refresh bound identity (`storeBoundDevice` / switch bound list) — verify: reachable On/Off and at least one non-On/Off kind can complete immediate rebind without Delete
- [x] 2.2 On no usable short, bind failure, or timeout: clear **that** IEEE from the switch bind cache, open permit-join, keep registry entry, emit pairing-fallback result — verify: log/result shows fallback and Search-style announce can bind again for that IEEE
- [x] 2.3 Reject or no-op safely when Zigbee is not started — verify: result is failure and permit-join is not left open indefinitely

## 3. Host API

- [x] 3.1 Add `POST /api/devices/rebind` (IEEE required) gated by edit-devices permission; reject unknown IEEE and MQTT transport — verify: 403/400 cases and Zigbee success/fallback responses
- [x] 3.2 Wait for the slave rebind result with a bounded timeout and map outcomes to the HTTP/JSON response — verify: immediate success vs pairing fallback are distinguishable to the client

## 4. Device dialog UI

- [x] 4.1 Add a **Rebind** button on the parameter dialog for edit of registered Zigbee devices only (hide for MQTT and unsaved Add); keep Save/Cancel/Export/Delete — verify: button visibility matrix in the dialog
- [x] 4.2 Wire Rebind to the API with busy state and success / pairing-fallback / failure text; do not close the dialog or delete the device — verify: after restore-style unbound device, Rebind either binds immediately or opens pairing without Delete
- [x] 4.3 Ensure the enabled Rebind button is not grayed when editable — verify: enabled styling matches other secondary dialog actions

## 5. Version bump on apply

- [x] 5.1 Bump `FIRMWARE_VERSION_HOST` and `FIRMWARE_VERSION_SLAVE` builds on apply (after propose `4.2.56` / `4.2.05`) when source lands; smoke host+slave build — verify: version strings in Status / boot log

## Workflow follow-up

- Archive the change after review when the human asks for `/opsx-archive`.
