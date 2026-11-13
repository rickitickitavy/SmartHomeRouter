# Proposal

## Why

After restoring full settings on another router, Zigbee devices reappear as registered but are not bound on the new coordinator. Operators must delete each device and pair/bind again, which drops the registry entry and forces a full re-add. That is slow and error-prone when many devices were already configured.

## What Changes

- Add a **Rebind** control on the Zigbee device parameter dialog (edit of a registered Zigbee device).
- **Rebind** keeps the registered device record (name, channels, topics, reportable data, etc.) and does not require Delete.
- On click, try an **immediate coordinator-initiated ZDO bind** using the stored IEEE and any usable NWK short / endpoints the slave already knows.
- If immediate bind cannot start or fails, **fall back** to a pairing-window rebind: clear that IEEE from the switch bind cache so a re-announce can bind, and open permit-join (same class of window as Search).
- Apply to **all Zigbee device kinds** on the shared bind/join path (not On/Off only). MQTT-transport devices do not get Rebind.
- Propose bumps: `FIRMWARE_VERSION_HOST` `4.2.55` → `4.2.56`; `FIRMWARE_VERSION_SLAVE` `4.2.04` → `4.2.05` (apply bumps again when source lands).

## Capabilities

### New Capabilities

- `zigbee-device-rebind`: Operator-triggered rebind of an already-registered Zigbee device (immediate ZDO bind, then pairing-window fallback) without deleting the host registry entry.

### Modified Capabilities

- `web-console`: Device parameter dialog exposes Rebind for registered Zigbee devices and reports rebind outcome / fallback pairing state.
- `host-slave-spi`: Host→slave rebind command and slave→host rebind result (success vs pairing fallback / failure).
- `zigbee-slave-radio`: Slave performs immediate bind for the target IEEE across device kinds; on failure opens permit-join and allows re-announce bind without removing the registered map entry.

## Impact

- Host UI: `data/index.html` parameter dialog actions; new `POST /api/devices/rebind` (or equivalent) gated like device edit.
- Host↔slave: new SPI command + result framing; host does not delete the device on rebind.
- Slave: ZigbeeCoordinator rebind path (ZDO bind / match-desc style bind for known shorts+endpoints, then permit-join fallback + bind-cache clear for that IEEE).
- No change to settings export/restore shape; this is an operational recovery action after restore, not a new persisted feature store.
